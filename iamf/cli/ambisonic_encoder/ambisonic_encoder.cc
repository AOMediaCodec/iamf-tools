/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/ambisonic_encoder/ambisonic_encoder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "Eigen/Core"
#include "absl/log/absl_check.h"
#include "iamf/cli/ambisonic_encoder/ambisonic_utils.h"

namespace iamf_tools {

AmbisonicEncoder::AmbisonicEncoder(size_t buffer_size_per_channel,
                                   size_t number_of_input_channels,
                                   size_t ambisonic_order)
    : buffer_size_per_channel_(buffer_size_per_channel),
      number_of_input_channels_(number_of_input_channels),
      number_of_output_channels_((ambisonic_order + 1) * (ambisonic_order + 1)),
      ambisonic_order_(ambisonic_order),
      alp_generator_(static_cast<int>(ambisonic_order), false, false) {
  ABSL_CHECK_GE(number_of_input_channels_, 0);
  ABSL_CHECK_GE(ambisonic_order_, 0);

  // Initialize the encoding matrix.
  encoding_matrix_ =
      Eigen::MatrixXf::Zero(static_cast<int>(number_of_output_channels_),
                            static_cast<int>(number_of_input_channels_));
}

void AmbisonicEncoder::SetSource(size_t input_channel, float gain,
                                 float azimuth, float elevation,
                                 float distance) {
  ABSL_CHECK_NE(number_of_input_channels_, 0);
  ABSL_CHECK_NE(number_of_output_channels_, 0);
  ABSL_CHECK_LT(input_channel, number_of_input_channels_);

  // Check if the key exists in the map.
  if (sources_.find(input_channel) == sources_.end()) {
    // Add the source to the map.
    sources_.insert({input_channel, {0.0, 0.0, 0.0, 0.0}});
  }

  // Check if the source is already set to these properties.
  if (sources_.at(input_channel).gain == gain &&
      sources_.at(input_channel).azimuth == azimuth &&
      sources_.at(input_channel).elevation == elevation &&
      sources_.at(input_channel).distance == distance) {
    return;
  }

  // Update the gain, azimuth, elevation and distance of the source.
  sources_.at(input_channel) = {gain, azimuth, elevation, distance};

  // Calculate the overall gain for the source. Limit the minimum distance to
  // 0.5 m.
  float overall_gain = gain / std::max(distance, 0.5f);

  // Mute the source if the overall gain is less than -120 dB.
  if (overall_gain < 0.000001) {
    encoding_matrix_.col(static_cast<int>(input_channel)).setZero();
    return;
  }

  // Get the spherical harmonic coefficients for the given azimuth and
  // elevation.
  std::vector<float> sh_coeffs(number_of_output_channels_);
  GetShCoeffs(azimuth, elevation, ambisonic_order_, sh_coeffs);

  // Scale the spherical harmonic coefficients by the overall gain and update
  // the encoding matrix.
  for (size_t i = 0; i < number_of_output_channels_; i++) {
    encoding_matrix_(static_cast<int>(i), static_cast<int>(input_channel)) =
        sh_coeffs.at(i) * overall_gain;
  }
}

void AmbisonicEncoder::RemoveSource(size_t input_channel) {
  // Remove the source from the map.
  sources_.erase(input_channel);

  // Mute the input channel in the encoding matrix.
  encoding_matrix_.col(static_cast<int>(input_channel)).setZero();
}

void AmbisonicEncoder::ProcessPlanarAudioData(
    const std::vector<float>& input_buffer,
    std::vector<float>& output_buffer) const {
  // Check if the input buffer size matches the declared buffer size and number
  // of input channels.
  ABSL_CHECK_EQ(input_buffer.size(),
                number_of_input_channels_ * buffer_size_per_channel_);

  // Check if the output buffer size matches the declared buffer size and number
  // of output channels.
  ABSL_CHECK_EQ(output_buffer.size(),
                number_of_output_channels_ * buffer_size_per_channel_);

  // Create Eigen map for the input buffer.
  const Eigen::Map<const Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic,
                                       Eigen::RowMajor>>
      input_matrix(input_buffer.data(),
                   static_cast<int>(number_of_input_channels_),
                   static_cast<int>(buffer_size_per_channel_));

  // Create Eigen map for the output buffer.
  Eigen::Map<
      Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
      output_matrix(output_buffer.data(),
                    static_cast<int>(number_of_output_channels_),
                    static_cast<int>(buffer_size_per_channel_));

  // Perform Ambisonic encoding.
  output_matrix = encoding_matrix_ * input_matrix;
}

void AmbisonicEncoder::GetShCoeffs(float azimuth, float elevation,
                                   size_t ambisonic_order,
                                   std::vector<float>& coeffs) {
  float azimuth_rad = azimuth * kRadiansFromDegrees;
  float elevation_rad = elevation * kRadiansFromDegrees;

  std::vector<float> associated_legendre_polynomials_temp_ =
      alp_generator_.Generate(std::sin(elevation_rad));
  // Compute the actual spherical harmonics using the generated polynomials.
  for (int degree = 0; degree <= ambisonic_order; degree++) {
    for (int order = -degree; order <= degree; order++) {
      const int row = AcnSequence(degree, order);
      if (row == -1) {
        // Skip this spherical harmonic.
        continue;
      }

      const float last_term =
          (order >= 0) ? std::cos(static_cast<float>(order) * azimuth_rad)
                       : std::sin(static_cast<float>(-order) * azimuth_rad);

      coeffs.at(row) =
          Sn3dNormalization(degree, order) *
          associated_legendre_polynomials_temp_[alp_generator_.GetIndex(
              degree, std::abs(order))] *
          last_term;
    }
  }
}

}  // namespace iamf_tools
