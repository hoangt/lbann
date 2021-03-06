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

#ifndef LBANN_LAYERS_TARGET_LAYER_HPP_INCLUDED
#define LBANN_LAYERS_TARGET_LAYER_HPP_INCLUDED

#include "lbann/layers/io/io_layer.hpp"
#include "lbann/utils/exception.hpp"
#include "lbann/models/model.hpp"
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace lbann {
class target_layer : public io_layer {
 protected:
  bool m_shared_data_reader;

 public:
  target_layer(lbann_comm *comm, std::map<execution_mode, generic_data_reader *> data_readers, bool shared_data_reader, bool for_regression = false)
    : io_layer(comm, data_readers, true, for_regression) {
    m_shared_data_reader = shared_data_reader;
  }

  virtual ~target_layer() {
    if (!m_shared_data_reader) {
      // Only free the data readers if they're not shared with the input layer.
      if (m_training_dataset.data_reader != nullptr) {
        delete m_training_dataset.data_reader;
      }
      if (m_validation_dataset.data_reader != nullptr) {
        delete m_validation_dataset.data_reader;
      }
      if (m_testing_dataset.data_reader != nullptr) {
        delete m_testing_dataset.data_reader;
      }
    }
  }

  template<data_layout T_layout> inline void initialize_distributed_matrices() {
    io_layer::initialize_distributed_matrices<T_layout>();
  }

  virtual void setup_dims() {
    io_layer::setup_dims();
    if (this->is_for_regression()) {
      this->m_neuron_dims = io_layer::get_data_dims();
      this->m_num_neuron_dims = this->m_neuron_dims.size();
      this->m_num_neurons = std::accumulate(this->m_neuron_dims.begin(),
                                            this->m_neuron_dims.end(),
                                            1,
                                            std::multiplies<int>());
    } else {
      this->m_num_neurons = io_layer::get_linearized_label_size();
      this->m_num_neuron_dims = 1;
      this->m_neuron_dims.assign(1, this->m_num_neurons);
    }
  }

  virtual void setup_data() {
    io_layer::setup_data();
    std::stringstream err;

    if(!this->m_shared_data_reader) { /// If the target layer shares a data reader with an input layer, do not setup the data reader a second time
      if(m_training_dataset.data_reader != nullptr) {
        m_training_dataset.data_reader->setup();
      }
      if(m_validation_dataset.data_reader != nullptr) {
        m_validation_dataset.data_reader->setup();
      }
      if(m_testing_dataset.data_reader != nullptr) {
        m_testing_dataset.data_reader->setup();
      }
    }

    if(this->m_num_prev_neurons != this->m_num_neurons) {
      err << __FILE__ << " " << __LINE__ 
          << " :: " << get_name() << " this->m_num_prev_neurons != this->m_num_neurons; this->m_num_prev_neurons= " << this->m_num_prev_neurons << " this->m_num_neurons= " << this->m_num_neurons << endl;
      throw lbann_exception(err.str());
    }

    if(this->m_neural_network_model->m_obj_fn == NULL) {
      err << __FILE__ << " " << __LINE__ 
          << " :: lbann_target_layer: target layer has invalid objective function pointer";
      throw lbann_exception(err.str());
    }
    this->m_neural_network_model->m_obj_fn->setup(
      this->m_num_neurons,
      this->m_neural_network_model->get_max_mini_batch_size());
    for (auto&& m : this->m_neural_network_model->get_metrics()) {
      m->setup(this->m_num_neurons,
               this->m_neural_network_model->get_max_mini_batch_size());
      m->m_neural_network_model = this->m_neural_network_model;
    }
  }

  lbann::generic_data_reader *set_training_data_reader(generic_data_reader *data_reader, bool shared_data_reader) {
    m_shared_data_reader = shared_data_reader;
    return io_layer::set_training_data_reader(data_reader);
  }

  lbann::generic_data_reader *set_testing_data_reader(generic_data_reader *data_reader, bool shared_data_reader) {
    m_shared_data_reader = shared_data_reader;
    return io_layer::set_testing_data_reader(data_reader);
  }

  void fp_set_std_matrix_view() {
    int cur_mini_batch_size = this->m_neural_network_model->get_current_mini_batch_size();
    Layer::fp_set_std_matrix_view();
    this->m_neural_network_model->m_obj_fn->fp_set_std_matrix_view(cur_mini_batch_size);
    for (auto&& m : this->m_neural_network_model->get_metrics()) {
      m->fp_set_std_matrix_view(cur_mini_batch_size);
    }
  }

  void summarize_stats(lbann_summary& summarizer, int step) {
    std::string obj_name = this->m_neural_network_model->m_obj_fn->name();
    // Replace spaces with _ for consistency.
    std::transform(obj_name.begin(), obj_name.end(), obj_name.begin(),
                   [] (char c) { return c == ' ' ? '_' : c; });
    std::string tag = "layer" + std::to_string(this->m_index) + "/objective_" +
      obj_name;
    summarizer.reduce_scalar(
      tag,
      this->m_neural_network_model->m_obj_fn->report_aggregate_avg_obj_fn(
        execution_mode::training), step);
    io_layer::summarize_stats(summarizer, step);
  }

  void epoch_print() const {
    double obj_cost = this->m_neural_network_model->m_obj_fn->report_aggregate_avg_obj_fn(execution_mode::training);
    if (this->m_comm->am_world_master()) {
      std::vector<double> avg_obj_fn_costs(this->m_comm->get_num_models());
      this->m_comm->intermodel_gather(obj_cost, avg_obj_fn_costs);
      for (size_t i = 0; i < avg_obj_fn_costs.size(); ++i) {
        std::cout << "Model " << i << " average " <<
          this->m_neural_network_model->m_obj_fn->name() << ": " <<
          avg_obj_fn_costs[i] << std::endl;
      }
    } else {
      this->m_comm->intermodel_gather(obj_cost, this->m_comm->get_world_master());
    }
  }

  void epoch_reset() {
    Layer::epoch_reset();
    resetCost();
  }

  void resetCost() {
    this->m_neural_network_model->m_obj_fn->reset_obj_fn();
  }

  bool saveToCheckpoint(int fd, const char *filename, size_t *bytes) {
    /// @todo should probably save m_shared_data_reader
    return Layer::saveToCheckpoint(fd, filename, bytes);
  }

  bool loadFromCheckpoint(int fd, const char *filename, size_t *bytes) {
    /// @todo should probably save m_shared_data_reader
    return Layer::loadFromCheckpoint(fd, filename, bytes);
  }

  bool saveToCheckpointShared(persist& p) {
    // rank 0 writes softmax cost to file
    if (p.get_rank() == 0) {
      // p.write_double(persist_type::train, "aggregate cost", (double) aggregate_cost);
      // p.write_uint64(persist_type::train, "num backprop steps", (uint64_t) num_backprop_steps);
    }

    return true;
  }

  bool loadFromCheckpointShared(persist& p) {
    // rank 0 writes softmax cost to file
    // if (p.get_rank() == 0) {
    //     double dval;
    //     p.read_double(persist_type::train, "aggregate cost", &dval);
    //     aggregate_cost = (DataType) dval;

    //     uint64_t val;
    //     p.read_uint64(persist_type::train, "num backprop steps", &val);
    //     num_backprop_steps = (long) val;
    // }

    // // get values from rank 0
    // MPI_Bcast(&aggregate_cost, 1, DataTypeMPI, 0, MPI_COMM_WORLD);
    // MPI_Bcast(&num_backprop_steps, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    //return Layer::loadFromCheckpointShared(dir, bytes);
    return true;
  }
};

}  // namespace lbann

#endif  // LBANN_LAYERS_TARGET_LAYER_HPP_INCLUDED
