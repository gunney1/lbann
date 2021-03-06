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
////////////////////////////////////////////////////////////////////////////////

#include "lbann/layers/activations/leaky_relu.hpp"

namespace lbann {

namespace {

// Useful constants
constexpr DataType zero = 0;

/** Local forward prop computation. */
void local_fp(DataType negative_slope,
              const AbsMat& input,
              AbsMat& output) {
  const auto& height = input.Height();
  const auto& width = input.Width();
  LBANN_OMP_PARALLEL_FOR_COLLAPSE2
  for (El::Int col = 0; col < width; ++col) {
    for (El::Int row = 0; row < height; ++row) {
      const auto& x = input(row, col);
      auto& y = output(row, col);
      y = (x > zero) ? x : negative_slope * x;
    }
  }
}

/** Local backprop computation. */
void local_bp(DataType negative_slope,
              const AbsMat& input,
              const AbsMat& gradient_wrt_output,
              AbsMat& gradient_wrt_input) {
  const auto& height = input.Height();
  const auto& width = input.Width();
  LBANN_OMP_PARALLEL_FOR_COLLAPSE2
  for (El::Int col = 0; col < width; ++col) {
    for (El::Int row = 0; row < height; ++row) {
      const auto& x = input(row, col);
      const auto& dy = gradient_wrt_output(row, col);
      auto& dx = gradient_wrt_input(row, col);
      dx = (x > zero) ? dy : negative_slope * dy;
    }
  }
}

} // namespace

template <>
void leaky_relu_layer<data_layout::DATA_PARALLEL, El::Device::CPU>
       ::fp_compute() {
  local_fp(m_negative_slope,
           get_local_prev_activations(),
           get_local_activations());
}
template <>
void leaky_relu_layer<data_layout::DATA_PARALLEL, El::Device::CPU>
     ::bp_compute() {
  local_bp(m_negative_slope,
           get_local_prev_activations(),
           get_local_prev_error_signals(),
           get_local_error_signals());
}
template <>
void leaky_relu_layer<data_layout::MODEL_PARALLEL, El::Device::CPU>
       ::fp_compute() {
  local_fp(m_negative_slope,
           get_local_prev_activations(),
           get_local_activations());
}
template <>
void leaky_relu_layer<data_layout::MODEL_PARALLEL, El::Device::CPU>
     ::bp_compute() {
  local_bp(m_negative_slope,
           get_local_prev_activations(),
           get_local_prev_error_signals(),
           get_local_error_signals());
}

} // namespace lbann
