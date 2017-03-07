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
// lbann_data_reader .hpp - Input data base class for training, testing
////////////////////////////////////////////////////////////////////////////////

#ifndef LBANN_DATA_READER_HPP
#define LBANN_DATA_READER_HPP

#include "lbann/lbann_base.hpp"
#include "lbann/utils/lbann_random.hpp"
#include "lbann/utils/lbann_exception.hpp"
#include "lbann/lbann_comm.hpp"
#include "lbann/io/lbann_file_io.hpp"
#include "lbann/io/lbann_persist.hpp"
#include <assert.h>
#include <algorithm>
#include <string>
#include <vector>
#include <unistd.h>


#define NOT_IMPLEMENTED(n) { \
  std::stringstream s; \
  s << "the method " << n << " has not been implemented"; \
  throw lbann_exception(s.str()); }

/**
 * @todo - add support for save and restore
 */
namespace lbann
{

class DataReader
{
public:
  DataReader(int batchSize, bool shuffle = true) :
    BatchSize(batchSize), CurrentPos(0), m_shuffle(shuffle),
    m_stride(batchSize), m_base_offset(0), m_model_offset(0), 
    m_use_alt_last_mini_batch_size(false),
    m_last_mini_batch_threshold(0), m_last_mini_batch_size(batchSize),
    m_last_mini_batch_stride(batchSize),
    m_file_dir(""), m_data_fn(""), m_label_fn(""),
    m_first_n(false), m_max_sample_count(0), m_validation_percent(-1),
    m_max_sample_count_was_set(false), m_use_percent(-1)
  {}
    
  //developer's note: I eliminated the copy ctor, since the
  //default does everything we need; eliminating our explicit
  //code helps minimize sources of error -dHysom

  virtual ~DataReader() {}

  /** @name Methods related to construction and loading
   *  These methods are used in drivers (front ends) to construct data readers,
   *  tell them were to find data, how much to load, etc.
   *  These are all non-virtual methods.
   */

  /** 
   * Set base directory for your data. Optional: if given,
   * then get_data_filename will concatenate the value passed
   * to this method with the value passed to set_data_filename,
   * and similarly for get_label_filename
   */
  void set_file_dir(std::string s);

  /**
   * Returns the base directory for your data. 
   * If set_file_dir was not called, returns the empty string
   */
  std::string get_file_dir();

  /**
   * Set the filename for your data (images, etc).
   * This may either be a complete filepath, or a subdirectory;
   * see note for set_file_dir(). Also, use this method
   * for cases where the file contains a list of files (e.g, imagenet)
   */ 
  void set_data_filename(std::string s);

  /**
   * Returns the complete filepath to you data file.
   * See not for set_file_dir()
   */
  std::string get_data_filename(); 

  /**
   * Set the filename for your data (images, etc).
   * This may either be a complete filepath, or a subdirectory;
   * see note for set_file_dir()
   */ 
  void set_label_filename(std::string s);

  /**
   * Returns the complete filepath to you data file.
   * See not for set_file_dir(). Note: some pipelines (autoencoders)
   * will not make use of this method.
   */
  std::string get_label_filename(); 

  /**
   * Use the first N data entries, without shuffling;
   * default is: false
   */
  void set_firstN(bool b);
  bool get_firstN();

  void set_max_sample_count(size_t s);
  bool has_max_sample_count();
  size_t get_max_sample_count();

  void set_use_percent(double s);
  bool has_use_percent();
  double get_use_percent();

  void set_validation_percent(double s);
  bool has_validation_percent();
  double get_validation_percent();

  /**
   * Pure abstract virtual function; all DataReaders *must* implement.
   */
  virtual void load() = 0;

  ///@}





  /**
   * Prepare to start processing an epoch of data.
   * If shuffle is true, then shuffle the indices of the data set
   * If the base offset is not specified set it to 0
   * If the stride is not specified set it to batch size
   */
  void setup(int base_offset, int stride, int model_offset = 0, lbann_comm *comm = NULL);
  void setup();

  virtual int fetch_data(Mat& X) { 
    NOT_IMPLEMENTED("fetch_data");
    return 0; 
  }

  virtual int fetch_label(Mat& Y) { 
    NOT_IMPLEMENTED("fetch_label");
    return 0; 
  }

  virtual int fetch_response(Mat& Y) { 
    NOT_IMPLEMENTED("fetch_response");
    return 0; 
  }

  virtual void save_image(Mat& pixels, const std::string filename, bool scale = true) { 
   NOT_IMPLEMENTED("save_image"); 
  }

  /**
   * During the network's update phase, the data reader will
   * advanced the current position pointer.  If the pointer wraps
   * around, then reshuffle the data indicies.
   */
  virtual bool update();

  virtual int getNumLabels() { return 0; }
  virtual int getNumResponses() { return 1; }
  virtual int get_linearized_data_size() { return 0; }
  virtual int get_linearized_label_size() { return 0; }
  virtual int get_linearized_response_size() { return 1; }

  bool position_valid() { return (CurrentPos < (int)ShuffledIndices.size()); }
  bool at_new_epoch() { return (m_current_mini_batch_idx == 0); }
  int getBatchSize();
  int getPosition() { return CurrentPos; }
  int get_next_position();
  int* getIndices() { return &ShuffledIndices[0]; }
  int getNumData() { return (int)ShuffledIndices.size(); }
  int get_num_unused_data() { return (int)m_unused_indices.size(); }
  int* get_unused_data() { return &m_unused_indices[0]; }

  void select_subset_of_data(size_t max_sample_count, bool firstN);

  bool swap_used_and_unused_index_sets();

  DataReader& operator=(const DataReader& source);

  size_t trim_data_set(double use_percentage, bool firstN=false);

  void calculate_multi_model_data_distribution(lbann_comm *comm);

  /** \brief Given directory to store checkpoint files, write state to file and add to number of bytes written */
  bool saveToCheckpointShared(persist& p, const char* name);

  /** \brief Given directory to store checkpoint files, read state from file and add to number of bytes read */
  bool loadFromCheckpointShared(persist& p, const char* name);

protected:
  int BatchSize;
  int CurrentPos;
  int m_shuffle;
  /// Stride is typically batch_size, but may be a multiple of batch size if there are multiple readers
  int m_stride;
  /// If there are multiple instances of the reader, 
  /// then it may not reset to zero
  int m_base_offset;
  /// If there are multiple models with multiple instances of the reader, 
  /// each model's set of readers may not reset to zero
  /// Provide a set of size, strides, and thresholds to handle the last mini batch of a dataset
  int m_model_offset;
  int m_use_alt_last_mini_batch_size;
  int m_last_mini_batch_threshold;
  int m_last_mini_batch_size;
  int m_last_mini_batch_stride;

  int m_current_mini_batch_idx;
  int m_num_mini_batches_per_reader;

  std::vector<int> ShuffledIndices;
  /// Record of the indicies that are not being used for training
  std::vector<int> m_unused_indices;

  std::string m_file_dir;
  std::string m_data_fn;
  std::string m_label_fn;
  bool m_first_n;
  size_t m_max_sample_count;
  double m_validation_percent;
  size_t m_max_sample_count_was_set;
  double m_use_percent;
};

}  // namespace lbann

#endif  // LBANN_DATA_READER_HPP
