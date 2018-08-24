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

#include "lbann/layers/transform/crop.hpp"
#include "lbann/utils/cuda.hpp"

namespace lbann {

namespace {

__global__ void fp_compute_3d_kernel(
  El::Int input_dimx, El::Int input_dimy, El::Int input_dimz,
  El::Int output_dimx, El::Int output_dimy, El::Int output_dimz,
  El::Int width,
  const DataType * __restrict__ input, int input_ldim,
        DataType * __restrict__ output, int output_ldim,
  const DataType * __restrict__ crop_pos, int crop_pos_ldim) {

  // Indices
  const El::Int gidx = threadIdx.x + blockIdx.x * blockDim.x;
  const El::Int bidy = blockIdx.y;
  const El::Int num_blocks_y = gridDim.y;
  const El::Int num_threads_x = blockDim.x * gridDim.x;

  const auto& output_size = output_dimx * output_dimy * output_dimz;
  const auto& num_offsets_x = input_dimx - output_dimx + 1;
  const auto& num_offsets_y = input_dimy - output_dimy + 1;
  const auto& num_offsets_z = input_dimz - output_dimz + 1;

  // Iterate through mini-batch samples
  for (El::Int col = bidy; col < width; col += num_blocks_y) {

    // Crop offsets
    const auto& offz = min(static_cast<El::Int>(num_offsets_z * crop_pos[3*col]),
                           num_offsets_z - 1);
    const auto& offy = min(static_cast<El::Int>(num_offsets_y * crop_pos[3*col+1]),
                           num_offsets_y - 1);
    const auto& offx = min(static_cast<El::Int>(num_offsets_x * crop_pos[3*col+2]),
                           num_offsets_x - 1);
    
    // Iterate through output entries in mini-batch sample
    for (El::Int output_pos = gidx;
         output_pos < output_size;
         output_pos += num_threads_x) {

      // Get output entry
      const auto& output_posx = output_pos % output_dimx;
      const auto& output_posy = (output_pos / output_dimx) % output_dimy;
      const auto& output_posz = output_pos / (output_dimx * output_dimy);
      auto& output_entry = output[output_pos + col * output_ldim];

      // Get input entry
      const auto& input_posx = output_posx + offx;
      const auto& input_posy = output_posy + offy;
      const auto& input_posz = output_posz + offz;
      const auto& input_pos = (input_posx
                               + input_posy * input_dimx
                               + input_posz * input_dimx * input_dimy);
      const auto& input_entry = input[input_pos + col * input_ldim];

      // Copy entry
      output_entry = input_entry;
      
    }
  }
  
}

__global__ void bp_compute_3d_kernel(
  El::Int input_dimx, El::Int input_dimy, El::Int input_dimz,
  El::Int output_dimx, El::Int output_dimy, El::Int output_dimz,
  El::Int width,
  const DataType * __restrict__ gradient_wrt_output, int gradient_wrt_output_ldim,
        DataType * __restrict__ gradient_wrt_input, int gradient_wrt_input_ldim,
  const DataType * __restrict__ crop_pos, int crop_pos_ldim) {

  // Indices
  const El::Int gidx = threadIdx.x + blockIdx.x * blockDim.x;
  const El::Int bidy = blockIdx.y;
  const El::Int num_blocks_y = gridDim.y;
  const El::Int num_threads_x = blockDim.x * gridDim.x;

  const auto& output_size = output_dimx * output_dimy * output_dimz;
  const auto& num_offsets_x = input_dimx - output_dimx + 1;
  const auto& num_offsets_y = input_dimy - output_dimy + 1;
  const auto& num_offsets_z = input_dimz - output_dimz + 1;

  // Iterate through mini-batch samples
  for (El::Int col = bidy; col < width; col += num_blocks_y) {

    // Crop offsets
    const auto& offz = min(static_cast<El::Int>(num_offsets_z * crop_pos[3*col]),
                           num_offsets_z - 1);
    const auto& offy = min(static_cast<El::Int>(num_offsets_y * crop_pos[3*col+1]),
                           num_offsets_y - 1);
    const auto& offx = min(static_cast<El::Int>(num_offsets_x * crop_pos[3*col+2]),
                           num_offsets_x - 1);
    
    // Iterate through output entries in mini-batch sample
    for (El::Int output_pos = gidx;
         output_pos < output_size;
         output_pos += num_threads_x) {

      // Get output entry
      const auto& output_posx = output_pos % output_dimx;
      const auto& output_posy = (output_pos / output_dimx) % output_dimy;
      const auto& output_posz = output_pos / (output_dimx * output_dimy);
      const auto& output_entry = gradient_wrt_output[output_pos + col * gradient_wrt_output_ldim];

      // Get input entry
      const auto& input_posx = output_posx + offx;
      const auto& input_posy = output_posy + offy;
      const auto& input_posz = output_posz + offz;
      const auto& input_pos = (input_posx
                               + input_posy * input_dimx
                               + input_posz * input_dimx * input_dimy);
      auto& input_entry = gradient_wrt_input[input_pos + col * gradient_wrt_input_ldim];

      // Copy entry
      input_entry = output_entry;
      
    }
  }
  
}
  
} // namespace
  
template <>
void crop_layer<data_layout::DATA_PARALLEL, El::Device::GPU>::fp_compute_3d() {

  // Local matrices
  const auto& local_input = get_local_prev_activations(0);
  const auto& local_crop_pos = get_local_prev_activations(1);
  auto& local_output = get_local_activations();

  // Tensor dimensions
  const auto& local_width = local_input.Width();
  const auto input_dims = get_input_dims();
  const auto output_dims = get_output_dims();
  const auto& output_size = get_output_size();

  // Launch CUDA kernel
  if (output_size > 0 && local_width > 0) {
    const int block_size = 256;
    dim3 block_dims, grid_dims;
    block_dims.x = block_size;
    grid_dims.x = (output_size + block_size - 1) / block_size;
    grid_dims.y = local_width;
    fp_compute_3d_kernel<<<grid_dims, block_dims, 0, El::GPUManager::Stream()>>>(
      input_dims[2], input_dims[1], input_dims[0],
      output_dims[2], output_dims[1], output_dims[0],
      local_width,
      local_input.LockedBuffer(), local_input.LDim(),
      local_output.Buffer(), local_output.LDim(),
      local_crop_pos.LockedBuffer(), local_crop_pos.LDim());
  }
  
}

template <>
void crop_layer<data_layout::DATA_PARALLEL, El::Device::GPU>::bp_compute_3d() {

  // Local matrices
  const auto& local_gradient_wrt_output = get_local_prev_error_signals();
  const auto& local_crop_pos = get_local_prev_activations(1);
  auto& local_gradient_wrt_input = get_local_error_signals(0);
  El::Zero(get_local_error_signals(1));
  
  // Tensor dimensions
  const auto& local_width = local_gradient_wrt_input.Width();
  const auto input_dims = get_input_dims();
  const auto output_dims = get_output_dims();
  const auto& output_size = get_output_size();

  // Launch CUDA kernel
  if (output_size > 0 && local_width > 0) {
    const int block_size = 256;
    dim3 block_dims, grid_dims;
    block_dims.x = block_size;
    grid_dims.x = (output_size + block_size - 1) / block_size;
    grid_dims.y = local_width;
    bp_compute_3d_kernel<<<grid_dims, block_dims, 0, El::GPUManager::Stream()>>>(
      input_dims[2], input_dims[1], input_dims[0],
      output_dims[2], output_dims[1], output_dims[0],
      local_width,
      local_gradient_wrt_output.LockedBuffer(), local_gradient_wrt_output.LDim(),
      local_gradient_wrt_input.Buffer(), local_gradient_wrt_input.LDim(),
      local_crop_pos.LockedBuffer(), local_crop_pos.LDim());
  }
  
}

} // namespace lbann
