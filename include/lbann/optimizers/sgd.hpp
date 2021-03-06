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

#ifndef LBANN_OPTIMIZERS_SGD_HPP
#define LBANN_OPTIMIZERS_SGD_HPP

#include "lbann/optimizers/optimizer.hpp"

namespace lbann {

/** Stochastic gradient descent optimizer.
 *
 *  Supports momentum and Nesterov acceleration.
 */
class sgd : public optimizer {

 public:

  sgd(lbann_comm *comm,
      DataType learning_rate,
      DataType momentum = 0,
      bool nesterov = false);

  sgd(const sgd& other);
  sgd& operator=(const sgd& other);
  ~sgd() override = default;
  sgd* copy() const override { return new sgd(*this); }

  /** Human-readable type name. */
  std::string get_type() const override { return "SGD"; }
  /** Human-readable description. */
  description get_description() const override;

  /** Velocity for momentum optimizer. */
  const AbsDistMat& get_velocity() const;
  /** Velocity for momentum optimizer. */
  AbsDistMat& get_velocity();

  void setup(weights& w) override;

  /** Perform the computation in an optimization step. */
  void step_compute(AbsDistMat& values, const AbsDistMat& gradient) override;
#ifdef LBANN_HAS_CUDNN
  /** Perform the computation in an optimization step on GPU. */
  void step_compute_gpu(AbsDistMat& values, const AbsDistMat& gradient) override;
#endif // LBANN_HAS_CUDNN

private:

  /** Momentum. */
  DataType m_momentum;
  /** Nesterov acceleration. */
  bool m_nesterov;
  /** Velocity for momentum optimizer. */
  std::unique_ptr<AbsDistMat> m_velocity;

//************************************************************************
// Checkpointing
//************************************************************************

  struct packing_header {
    DataType momentum;
  };

  bool pack_scalars(persist& p) {
    p.write_datatype(persist_type::train, "momentum", m_momentum);
    return true;
  }

  bool unpack_scalars(persist& p, struct packing_header *header){
    p.read_datatype(persist_type::train, "momentum",  &m_momentum);

    if(header != nullptr){
      header->momentum = m_momentum;
    }

  return true;
  }

  void unpack_header(struct packing_header& header){
    m_momentum = header.momentum;
  }

  bool save_to_checkpoint_shared(persist& p, std::string m_name) override;
  bool load_from_checkpoint_shared(persist& p, std::string m_name) override;

  bool save_to_checkpoint_distributed(persist& p, std::string m_name) override;
  bool load_from_checkpoint_distributed(persist& p, std::string m_name) override;

};

} // namespace lbann

#endif // LBANN_OPTIMIZERS_SGD_HPP
