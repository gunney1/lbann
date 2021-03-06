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

#include "lbann/models/directed_acyclic_graph.hpp"
#include <unordered_map>

namespace lbann {

directed_acyclic_graph_model::directed_acyclic_graph_model(lbann_comm *comm,
                                                           int mini_batch_size,
                                                           objective_function *obj_fn,
                                                           optimizer* default_optimizer)
  : model(comm, mini_batch_size, obj_fn, default_optimizer) {}

void directed_acyclic_graph_model::setup_layer_execution_order() {

  // Construct layer graph
  // Note: Each layer depends on its parent layers and its hint layer.
  std::set<int> nodes;
  std::map<int,std::set<int>> edges;
  const int num_layers = m_layers.size();
  std::unordered_map<const Layer *,int> layer_indices;
  for (int node = 0; node < num_layers; ++node) {
    nodes.insert(node);
    layer_indices[m_layers[node]] = node;
  }
  for (int node = 0; node < num_layers; ++node) {
    const auto& l = m_layers[node];
    for (const auto& child : l->get_child_layers()) {
      edges[node].insert(layer_indices[child]);
    }
    if (l->get_hint_layer() != nullptr) {
      edges[layer_indices[l->get_hint_layer()]].insert(node);
    }
  }

  // Topologically sort layers
  const auto& sorted_order = graph::topological_sort(nodes, edges);
  permute_layers(sorted_order);
  model::setup_layer_execution_order();

}

}  // namespace lbann
