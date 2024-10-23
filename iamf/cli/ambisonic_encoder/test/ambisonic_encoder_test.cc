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
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

// Test the Ambisonic encoder class for a number of single sources, 3OA output.
TEST(AmbisonicEncoderTest, TestOneSampleBufferOneSource) {
  const size_t buffer_size = 1;
  const int number_of_input_channels = 1;
  const int ambisonic_order = 3;

  const absl::flat_hash_map<std::pair<float, float>, std::vector<float>>
      expected_output = {
          // clang-format off
{{ 0.000000000000f,  0.000000000000f},
 { 1.000000000000f,  0.000000000000f,  0.000000000000f,  1.000000000000f,
   0.000000000000f,  0.000000000000f, -0.500000000000f,  0.000000000000f,
   0.866025403784f,  0.000000000000f,  0.000000000000f,  0.000000000000f,
   0.000000000000f, -0.612372435696f,  0.000000000000f,  0.790569415042f}},
{{-45.00000000000f,  30.00000000000f},
 { 1.000000000000f, -0.612372435696f,  0.500000000000f,  0.612372435696f,
  -0.649519052838f, -0.530330085890f, -0.125000000000f,  0.530330085890f,
   0.000000000000f, -0.363092188707f, -0.726184377414f, -0.093750000000f,
  -0.437500000000f,  0.093750000000f,  0.000000000000f, -0.363092188707f}},
{{12.000000000000f,  0.000000000000f},
 { 1.000000000000f,  0.207911690818f,  0.000000000000f,  0.978147600734f,
   0.352244265554f,  0.000000000000f, -0.500000000000f,  0.000000000000f,
   0.791153573830f,  0.464685043075f,  0.000000000000f, -0.127319388516f,
   0.000000000000f, -0.598990628731f,  0.000000000000f,  0.639584092002f}},
{{120.00000000000f, -90.00000000000f},
 { 1.000000000000f,  0.000000000000f, -1.000000000000f,  0.000000000000f,
   0.000000000000f,  0.000000000000f,  1.000000000000f,  0.000000000000f,
   0.000000000000f,  0.000000000000f,  0.000000000000f,  0.000000000000f,
  -1.000000000000f,  0.000000000000f,  0.000000000000f,  0.000000000000f}},
          // clang-format on
      };

  // Evaluation precision.
  const float kEpsilon = 1e-7;

  // Run the test.
  for (const auto& pair : expected_output) {
    auto tested_direction = pair.first;

    // Create an Ambisonic encoder object.
    AmbisonicEncoder encoder(buffer_size, number_of_input_channels,
                             ambisonic_order);

    // Add a source with a given direction.
    encoder.SetSource(0, 1.0f, tested_direction.first, tested_direction.second,
                      1.0f);

    // Create input buffer with 1 channel.
    std::vector<float> input_buffer(number_of_input_channels * buffer_size,
                                    0.0f);

    // Fill input buffer with ones.
    std::fill(input_buffer.begin(), input_buffer.end(), 1.0f);

    // Create output buffer with 16 channels.
    std::vector<float> output_buffer(
        (ambisonic_order + 1) * (ambisonic_order + 1) * buffer_size, 0.0f);

    encoder.ProcessPlanarAudioData(input_buffer, output_buffer);

    // Check if the output buffer matches the expected output buffer.
    for (size_t i = 0; i < output_buffer.size(); i++) {
      EXPECT_NEAR(output_buffer[i], pair.second[i], kEpsilon);
    }
  }
}

// Measure execution time of coefficient calculation using both implemented
// methods.
TEST(AmbisonicEncoderTest, MeasureExecutionTime) {
  const size_t buffer_size = 1;
  const int number_of_input_channels = 512;
  const int ambisonic_order = 7;

  // TODO(b/373302873): Replace this non-deterministic test. Use fixed values
  //                    if measuring execution time is all we need.
  // Create an array of azimuth/elevation pairs with random directions.
  std::vector<std::pair<float, float>> directions;
  for (int i = 0; i < number_of_input_channels; i++) {
    float azimuth =
        static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 360.0f;
    float elevation =
        static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 180.0f -
        90.0f;
    directions.push_back(std::make_pair(azimuth, elevation));
  }

  // Create an Ambisonic encoder object.
  AmbisonicEncoder encoder(buffer_size, number_of_input_channels,
                           ambisonic_order);

  // Start the timer.
  auto start = std::chrono::high_resolution_clock::now();

  // Assign sources to the encoder at all available input channels.
  for (int i = 0; i < number_of_input_channels; i++) {
    encoder.SetSource(i, 1.0f, directions[i].first, directions[i].second, 1.0f);
  }

  // Stop the timer.
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed_seconds = end - start;
  RecordProperty("Generate", elapsed_seconds.count());
}

}  // namespace
}  // namespace iamf_tools
