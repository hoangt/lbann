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
// lbann_early_stopping .hpp .cpp - Callback hooks for early stopping
////////////////////////////////////////////////////////////////////////////////

#include "lbann/callbacks/callback_early_stopping.hpp"

namespace lbann {

lbann_callback_early_stopping::lbann_callback_early_stopping(int64_t patience) :
  lbann_callback(), m_patience(patience) {}

/// Monitor the objective function to see if the validation score
/// continues to improve
void lbann_callback_early_stopping::on_validation_end(model *m) {
  double score = m->m_obj_fn->report_aggregate_avg_obj_fn(execution_mode::validation);
  if (score < m_last_score) {
    if (m->get_comm()->am_model_master()) {
      std::cout << "Model " << m->get_comm()->get_model_rank() <<
        " early stopping: score is improving " << m_last_score << " >> " <<
        score << std::endl;
    }
    m_last_score = score;
    m_wait = 0;
  } else {
    if (m_wait >= m_patience) {
      m->set_terminate_training(true);
      if (m->get_comm()->am_model_master()) {
        std::cout << "Model " << m->get_comm()->get_model_rank() <<
          " terminating training due to early stopping: " << score <<
          " score and " << m_last_score << " last score" << std::endl;
      }
    } else {
      ++m_wait;
    }
  }
}

}  // namespace lbann
