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
// patchworks_patch_descriptor.hpp - LBANN PATCHWORKS header for patch descriptor
////////////////////////////////////////////////////////////////////////////////

/**
 * LBANN PATCHWORKS header for patch descriptor
 */

#ifdef __LIB_OPENCV
#ifndef _PATCHWORKS_PATCH_DESCRIPTOR_H_INCLUDED_
#define _PATCHWORKS_PATCH_DESCRIPTOR_H_INCLUDED_

#include <string>
#include <ostream>
#include "patchworks_common.hpp"
#include "patchworks_opencv.hpp"
#include "patchworks_ROI.hpp"

namespace lbann {
namespace patchworks {

class patch_descriptor {
 public:
  unsigned int m_width; ///< patch width
  unsigned int m_height; ///< patch height
  unsigned int m_gap; ///< gap between patches
  unsigned int m_jitter; ///< patch position randomization

  /** patch centering mode
   *  0: place the center patch anywhere within the image
   *  1: place the center patch anywhere allowing the space for all 8 neighbor patches
   *  the rest: place the center patch at the center of the image
   */
  unsigned int m_mode_center;

  /** chromatic aberration correction mode
   *  0: nothing
   *  1: pixel transform px*B where a=[-1 2 -1] and B=I-a'a/(aa')
   *  2: randomly replace two channels with white noise
   */
  unsigned int m_mode_chrom;

  ROI m_sample_area; ///< The area to sample patches from
  ROI m_patch_center; ///< The center patch region
  std::string m_ext; ///< The file extension name (i.e., image type)

  /// The index of displacement used to generate the current patch
  unsigned int m_cur_patch_idx;

  /// The list of displacements to used to generate consecutive patches
  std::vector<displacement_type> m_displacements;
  /// The actual patch positions
  std::vector<ROI> m_positions;

 public:
  patch_descriptor() {
    init();  ///< Default constructor
  }
  virtual ~patch_descriptor() {}
  void init(); ///< Initializer

  /// Set patch size
  void set_size(const int w, const int h);
  /// Set the gap between neighboring patches
  void set_gap(const unsigned int g) {
    m_gap = g;
  }
  /// Set poisiton radomization parameter, the maximum jitter
  void set_jitter(const unsigned int j);

  /// Set mode to place center patch
  void set_mode_centering(const unsigned int m) {
    m_mode_center = m;
  }

  /// Set correction mode for chromatic aberration
  void set_mode_chromatic_aberration(const unsigned int m) {
    m_mode_chrom = m;
  }

  /// Declare the size of the image to take patches from, and implicitly set the area to sample as the entire image
  bool set_sample_image(const unsigned int w, const unsigned int h);
  /// Explicitly set the area to sample patches
  bool set_sample_area(const ROI& area);

  /// Set the file extention of patch files
  void set_file_ext(const std::string e) {
    m_ext = e;
  }

  /// A function that populates the list of displacements from the base patch to the next one
  virtual void define_patch_set();

  /// transform each pixel by B = I - a'*a/(a*a') where a=[-1 2 -1] to mitigate chromatic aberration
  bool is_to_correct_chromatic_aberration_at_pixel() const {
    return (m_mode_chrom == 1);
  }

  /// randomly drop two channels to avoid chromatic aberration impact
  bool is_to_drop_2channels() const {
    return (m_mode_chrom == 2);
  }

  /// Allow read-only access to the patch displacements
  const std::vector<displacement_type>& get_displacements() const {
    return m_displacements;
  }

  /// Compute the position of the first patch
  virtual bool get_first_patch(ROI& patch);
  /// Compute the position of a subsequent patch
  virtual bool get_next_patch(ROI& patch);
  /// extract all the patches defined
  virtual bool extract_patches(const cv::Mat& img, std::vector<cv::Mat>& patches);
  /// return the id (vector index + 1) of the last patch generated
  virtual unsigned int get_current_patch_idx() const { return m_cur_patch_idx; }

  /// Allow read-only access to the positions of the patches generated
  const std::vector<ROI>& access_positions() const {
    return m_positions;
  }
  /// print out the content of patch descriptor
  virtual std::ostream& print(std::ostream& os) const;
};

/// stream out the patch descriptor content
std::ostream& operator<<(std::ostream& os, const patch_descriptor& pd);

} // end of namespace patchworks
} // end of namespace lbann
#endif // _PATCHWORKS_PATCH_DESCRIPTOR_H_INCLUDED_
#endif // __LIB_OPENCV
