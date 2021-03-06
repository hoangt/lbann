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
// lbann_layer_softmax .hpp .cpp - softmax layer
////////////////////////////////////////////////////////////////////////////////

#ifndef LBANN_LAYER_SOFTMAX_HPP_INCLUDED
#define LBANN_LAYER_SOFTMAX_HPP_INCLUDED

#include "lbann/layers/activations/activation.hpp"
#include "lbann/layers/layer.hpp"
#include "lbann/io/file_io.hpp"
#include "lbann/utils/random.hpp"
#include "lbann/models/model.hpp"
#include "lbann/layers/io/target/target_layer_distributed_minibatch.hpp"
#include "lbann/layers/io/target/target_layer_partitioned_minibatch.hpp"
#include "lbann/objective_functions/objective_fn_categorical_cross_entropy.hpp"
#include <unistd.h>
#include <string>
#include <typeinfo>
#include <typeindex>

namespace lbann {

template <data_layout T_layout>
class softmax_layer : public activation_layer {

 private:
  AbsDistMat *m_workspace;
  AbsDistMat *m_workspace_v;

 public:
  softmax_layer(int index,
                lbann_comm *comm)
     :  activation_layer(index, comm) {
    initialize_distributed_matrices();
  }

  softmax_layer(const softmax_layer& other) :
    activation_layer(other) {
    m_workspace = other.m_workspace->Copy();
    m_workspace_v = other.m_workspace_v->Copy();
  }

  softmax_layer& operator=(const softmax_layer& other) {
    activation_layer::operator=(other);
    if (m_workspace) {
      delete m_workspace;
      delete m_workspace_v;
    }
    m_workspace = other.m_workspace->Copy();
    m_workspace_v = other.m_workspace_v->Copy();
  }

  ~softmax_layer() {
    delete m_workspace;
    delete m_workspace_v;
  }

  softmax_layer* copy() const { return new softmax_layer(*this); }

  std::string get_name() const { return "softmax"; }

  virtual inline void initialize_distributed_matrices();
  virtual data_layout get_data_layout() const { return T_layout; }

  void setup_data() {
    activation_layer::setup_data();
    m_workspace->Resize(
      1, this->m_neural_network_model->get_max_mini_batch_size());
  }

  void fp_set_std_matrix_view() {
    Int cur_mini_batch_size = this->m_neural_network_model->get_current_mini_batch_size();
    Layer::fp_set_std_matrix_view();
    El::View(*m_workspace_v, *m_workspace, El::ALL, El::IR(0, cur_mini_batch_size));
  }

  void fp_compute() {

    // Get local matrices and parameters
    Mat& workspace_local = m_workspace_v->Matrix();
    const Mat& prev_activations_local = this->m_prev_activations->LockedMatrix();
    Mat& activations_local = this->m_activations_v->Matrix();
    const Int local_height = activations_local.Height();
    const Int local_width = activations_local.Width();

    // Find maximum entry in each column
    #pragma omp parallel for
    for(El::Int col = 0; col < local_width; ++col) {
      DataType max_entry = prev_activations_local(El::Int(0), col);
      for(El::Int row = 1; row < local_height; ++row) {
        max_entry = std::max(max_entry, prev_activations_local(row,col));
      }
      workspace_local(El::Int(0), col) = max_entry;
    }
    AllReduce(*m_workspace_v, m_workspace_v->RedundantComm(), El::mpi::MAX);

    // Exponentiate activations and compute column sums
    // Note: Subtracting by the column max prevents activations from
    //   blowing up. Large negative values underflow to 0.
    #pragma omp parallel for
    for (El::Int col = 0; col < local_width; ++col) {
      const DataType shift = workspace_local(0, col);
      DataType sum = 0;
      for (El::Int row = 0; row < local_height; ++row) {
        const DataType prev_activations_entry = prev_activations_local(row, col);
        const DataType activations_entry = El::Exp(prev_activations_entry - shift);
        activations_local(row, col) = activations_entry;
        sum += activations_entry;
      }
      workspace_local(El::Int(0), col) = sum;
    }
    AllReduce(*m_workspace_v, m_workspace_v->RedundantComm(), El::mpi::SUM);

    // Divide activations by column sums
    // This truncates small values to 0 to avoid them becoming denormalized later
    // in the forward/backward stages. Denormalized values can significantly
    // impact floating point performance.
    El::IndexDependentMap(activations_local,
                          (std::function<DataType(El::Int,El::Int,const DataType&)>)
                          ([this,&workspace_local](El::Int r, El::Int c, const DataType& z)->DataType {
                            const DataType v = z / workspace_local(Int(0), c);
                            return Abs(v) < DataType(1e-8) ? DataType(1e-8) : v;
                          }));
  }

  // Defined below to avoid circular definitions.
  void bp_compute();

  bool update_compute() {
    return true;
  }

  bool saveToCheckpoint(int fd, const char *filename, size_t *bytes) {
    return Layer::saveToCheckpoint(fd, filename, bytes);
  }

  bool loadFromCheckpoint(int fd, const char *filename, size_t *bytes) {
    return Layer::loadFromCheckpoint(fd, filename, bytes);
  }

  bool saveToCheckpointShared(lbann::persist& p) {
    return Layer::saveToCheckpointShared(p);
  }

  bool loadFromCheckpointShared(lbann::persist& p) {
    return Layer::loadFromCheckpointShared(p);
  }
};

/// Matrices should be in MC,MR distributions
template<> inline void softmax_layer<data_layout::MODEL_PARALLEL>::initialize_distributed_matrices() {
  activation_layer::initialize_distributed_matrices<data_layout::MODEL_PARALLEL>();
  m_workspace = new StarMRMat(this->m_comm->get_model_grid());
  m_workspace_v = new StarMRMat(this->m_comm->get_model_grid());
}

/// Weight matrices should be in Star,Star and data matrices Star,VC distributions
template<> inline void softmax_layer<data_layout::DATA_PARALLEL>::initialize_distributed_matrices() {
  activation_layer::initialize_distributed_matrices<data_layout::DATA_PARALLEL>();
  m_workspace = new StarVCMat(this->m_comm->get_model_grid());
  m_workspace_v = new StarVCMat(this->m_comm->get_model_grid());
}

// Definited outside of the class to avoid circular dependencies with
// categorical_cross_entropy.
template <data_layout T_layout>
void softmax_layer<T_layout>::bp_compute() {
  // Stop early if objective function is categorical cross entropy
  // Note: error signal is already computed in objective function object
  const std::type_info& obj_fn_type = typeid(*(this->m_neural_network_model->m_obj_fn));
  const std::type_info& next_layer_type = typeid(*m_next_layer);
  if (std::type_index(obj_fn_type) ==
      std::type_index(typeid(objective_functions::categorical_cross_entropy))
      && (std::type_index(next_layer_type) ==
          std::type_index(typeid(target_layer_distributed_minibatch<data_layout::MODEL_PARALLEL>))
          || std::type_index(next_layer_type) ==
          std::type_index(typeid(target_layer_distributed_minibatch<data_layout::DATA_PARALLEL>))
          || std::type_index(next_layer_type) ==
          std::type_index(typeid(target_layer_partitioned_minibatch<data_layout::DATA_PARALLEL>)))) {
    El::View(*this->m_error_signal_v, *this->m_prev_error_signal);
    return;
  }

  // Get local matrices and parameters
  const Mat& activations_local = this->m_activations_v->LockedMatrix();
  const Mat& prev_error_signal_local = this->m_prev_error_signal->Matrix();
  Mat& error_signal_local = this->m_error_signal_v->Matrix();
  Mat& workspace_local = m_workspace_v->Matrix();
  const Int local_width = activations_local.Width();

  // Compute dot products
  // Note: prev_error_signal^T activations
  for(El::Int c=0; c<local_width; ++c) {
    workspace_local(El::Int(0), c) = El::Dot(prev_error_signal_local(El::ALL,El::IR(c)),
                                             activations_local(El::ALL,El::IR(c)));
  }
  AllReduce(*m_workspace_v, m_workspace_v->RedundantComm(), El::mpi::SUM);

  // Update error signal
  // Note: error_signal := activations * (prev_error_signal - prev_error_signal^T activations)
  El::IndexDependentMap(error_signal_local,
                        (std::function<DataType(El::Int,El::Int,const DataType&)>)
                        ([this,&activations_local,&workspace_local]
                         (El::Int r, El::Int c, const DataType& z)->DataType {
                          const DataType activations_entry = activations_local(r,c);
                          const DataType dot_product_entry = workspace_local(Int(0),c);
                          return activations_entry * (z - dot_product_entry);
                        }));
}

}  // namespace lbann

#endif  // LBANN_LAYER_SOFTMAX_HPP_INCLUDED
