////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2016, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
//
// lbann_data_reader_mnist .hpp .cpp - generic_data_reader class for MNIST dataset
////////////////////////////////////////////////////////////////////////////////

#include "lbann/data_readers/data_reader_mnist.hpp"
#include <stdio.h>

using namespace std;
using namespace El;

inline void __swapEndianInt(unsigned int& ui) {
  ui = ((ui >> 24) | ((ui<<8) & 0x00FF0000) | ((ui>>8) & 0x0000FF00) | (ui << 24));
}



lbann::mnist_reader::mnist_reader(int batchSize, bool shuffle)
  : generic_data_reader(batchSize, shuffle)
    //, lbann_image_preprocessor()
{
  m_image_width = 28;
  m_image_height = 28;
  m_num_labels = 10;
}

lbann::mnist_reader::mnist_reader(int batchSize)
  : mnist_reader(batchSize, true) {}

/*
lbann::mnist_reader::mnist_reader(const mnist_reader& source)
  : generic_data_reader((const generic_data_reader&) source),
    //lbann_image_preprocessor((const lbann_image_preprocessor&) source),
    m_image_width(source.m_image_width), m_image_height(source.m_image_height),
    m_num_labels(source.m_num_labels)
{
  // No need to deallocate data on a copy constuctor

  clone_image_data(source);
}
*/

lbann::mnist_reader::~mnist_reader(void) {
  //this->free();
}

int lbann::mnist_reader::fetch_data(Mat& X) {
  if(!generic_data_reader::position_valid()) {
    stringstream err;
    err << __FILE__<<" "<<__LINE__<< " :: MNIST data reader load error: !position_valid";
    throw lbann_exception(err.str());
  }

  int pixelcount = m_image_width * m_image_height;
  int current_batch_size = getm_batch_size();

  int n = 0, s = 0;
  std::vector<float> pixels(pixelcount);
  Zeros(m_indices_fetched_per_mb, X.Width(), 1); /// The width of the local matrix X dictates how many samples this rank will get
  for (n = m_current_pos, s = 0; n < m_current_pos + current_batch_size && s < X.Width() && n < m_shuffled_indices.size(); n+=m_sample_stride, s++) {
    //    std::cout << " Input Fetching " << n << " with batch size " << current_batch_size << " position " << m_current_pos << " and stride " << m_sample_stride << " and offset " << s << " which is sample index " << m_shuffled_indices[n] << " and the guard is " << (n < (m_current_pos + current_batch_size)) <<  " and there are only samples = " << getNumData() << std::endl;

    // if(n >= m_current_pos + current_batch_size) {
    //   std::cout << "WARNING: Input Fetching " << n << " with batch size " << current_batch_size << " and stride " << m_sample_stride << " and offset " << s << " which is sample index " << m_shuffled_indices[n] << " and the guard is " << (n < (m_current_pos + current_batch_size)) << " and the limit is " << (m_current_pos + current_batch_size) << std::endl;

    // }

    if (n >= (int)m_shuffled_indices.size()) {
      break;
    }

    int index = m_shuffled_indices[n];
    vector<unsigned char>& tmp = m_image_data[index];

    m_indices_fetched_per_mb.Set(s, 0, index);

    for (int p = 0; p < pixelcount; p++) {
      X.Set(p, s, tmp[p+1]);
    }

    auto pixel_col = X(IR(0, X.Height()), IR(s, s + 1));
    augment(pixel_col, m_image_height, m_image_width, 1);
    normalize(pixel_col, 1);
  }

  return s;
}

int lbann::mnist_reader::fetch_label(Mat& Y) {
  if(!generic_data_reader::position_valid()) {
    stringstream err;
    err << __FILE__<<" "<<__LINE__<< " :: MNIST data reader load error: !position_valid";
    throw lbann_exception(err.str());
  }

  int current_batch_size = getm_batch_size();
  int n = 0, s = 0;
  for (n = m_current_pos, s = 0; n < m_current_pos + current_batch_size && s < Y.Width() && n < m_shuffled_indices.size(); n+=m_sample_stride, s++) {
    if (n >= (int)m_shuffled_indices.size()) {
      break;
    }

    //std::cout << " Target Fetching " << n << " with batch size " << current_batch_size << " and stride " << m_sample_stride << " and offset " << s << std::endl;

    int index = m_shuffled_indices[n];
    unsigned char label = m_image_data[index][0];

    Y.Set(label, s, 1);
  }

  return s;
}



//===================================================

void lbann::mnist_reader::load(void) {
  if (is_master()) {
    cerr << "starting lbann::mnist_reader::load\n";
  }
  m_image_data.clear();

  string FileDir = get_file_dir();
  string ImageFile = get_data_filename();
  string LabelFile = get_label_filename();

  // set filepath
  string imagepath = FileDir + __DIR_DELIMITER + ImageFile;
  string labelpath = FileDir + __DIR_DELIMITER + LabelFile;

  // read labels
  FILE *fplbl = fopen(labelpath.c_str(), "rb");
  if (!fplbl) {
    stringstream err;
    err << __FILE__<<" "<<__LINE__<< " :: MNIST data reader: failed to open file: " << labelpath;
    throw lbann_exception(err.str());
  }

  int magicnum1, numitems1;
  fread(&magicnum1, 4, 1, fplbl);
  fread(&numitems1, 4, 1, fplbl);
  __swapEndianInt((unsigned int&)magicnum1);
  __swapEndianInt((unsigned int&)numitems1);

  // read images
  FILE *fpimg = fopen(imagepath.c_str(), "rb");
  if (!fpimg) {
    stringstream err;
    err << __FILE__<<" "<<__LINE__<< " :: MNIST data reader: failed to open file: " << imagepath;
    throw lbann_exception(err.str());
  }

  int magicnum2, numitems2, imgwidth, imgheight;
  fread(&magicnum2, 4, 1, fpimg);
  fread(&numitems2, 4, 1, fpimg);
  fread(&imgwidth, 4, 1, fpimg);
  fread(&imgheight, 4, 1, fpimg);
  __swapEndianInt((unsigned int&)magicnum2);
  __swapEndianInt((unsigned int&)numitems2);
  __swapEndianInt((unsigned int&)imgwidth);
  __swapEndianInt((unsigned int&)imgheight);

  if (numitems1 != numitems2) {
    fclose(fplbl);
    fclose(fpimg);
    stringstream err;
    err << __FILE__<<" "<<__LINE__<< " :: MNIST data reader: numitems1 != numitems2";
    throw lbann_exception(err.str());
  }

  // set to array
  m_image_data.resize(numitems1);
  for (int n = 0; n < numitems1; n++) {
    m_image_data[n].resize(1+(imgwidth * imgheight));
    fread(&m_image_data[n][0], 1, 1, fplbl);
    fread(&m_image_data[n][1], imgwidth * imgheight, 1, fpimg);
  }
  fclose(fpimg);
  fclose(fplbl);

  // reset indices
  m_shuffled_indices.clear();
  m_shuffled_indices.resize(m_image_data.size());
  for (size_t n = 0; n < m_shuffled_indices.size(); n++) {
    m_shuffled_indices[n] = n;
  }
  if (is_master()) {
    cerr << "calling select_subset_of_data; m_shuffled_indices.size: " << m_shuffled_indices.size() << endl;
  }
  select_subset_of_data();
}
