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
// lbann_cv_colorizer .cpp .hpp - transform a non-color (grayscale) image into a
//                               3-channel color image
////////////////////////////////////////////////////////////////////////////////

#include "lbann/data_readers/cv_colorizer.hpp"
#include "lbann/utils/mild_exception.hpp"

#ifdef __LIB_OPENCV
namespace lbann {

cv_colorizer::cv_colorizer(const cv_colorizer& rhs)
  : cv_transform(rhs), m_gray(rhs.m_gray) {}

cv_colorizer& cv_colorizer::operator=(const cv_colorizer& rhs) {
  cv_transform::operator=(rhs);
  m_gray = rhs.m_gray;
  return *this;
}

bool cv_colorizer::determine_transform(const cv::Mat& image) {
  m_enabled = m_gray = (!image.empty() && (image.channels() == 1));
  return m_enabled;
}

bool cv_colorizer::determine_inverse_transform() {
  m_enabled = m_gray;
  m_gray = false;
  return true;
}

bool cv_colorizer::apply(cv::Mat& image) {
  if (!m_enabled) {
    return true;
  }
  _LBANN_SILENT_EXCEPTION(image.empty(), "", false);

  if (!m_gray) {
    cv::Mat image_dst;
    cv::cvtColor(image, image_dst, cv::COLOR_BGR2GRAY);
    image = image_dst;
  } else {
    cv::Mat image_dst;
    cv::cvtColor(image, image_dst, cv::COLOR_GRAY2BGR);
    image = image_dst;
  }

  m_enabled = false;
  return true;
}

cv_colorizer *cv_colorizer::clone() const {
  return (new cv_colorizer(*this));
}

std::ostream& operator<<(std::ostream& os, const cv_colorizer& tr) {
  tr.print(os);
  return os;
}

} // end of namespace lbann
#endif // __LIB_OPENCV
