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

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

#include "benchmark/benchmark.h"
#include "iamf/cli/ambisonic_encoder/ambisonic_encoder.h"

namespace iamf_tools {
namespace {

// Measure execution time of coefficient calculation using both implemented
// methods.
static void BM_SHCalculation(benchmark::State& state) {
  const size_t buffer_size = 1;
  const int number_of_input_channels = 512;
  const int ambisonic_order = 7;

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

  for (auto _ : state) {
    // Assign sources to the encoder at all available input channels.
    for (int i = 0; i < number_of_input_channels; i++) {
      encoder.SetSource(i, 1.0f, directions[i].first, directions[i].second,
                        1.0f);
    }
  }
}

BENCHMARK(BM_SHCalculation)->Args({0})->Args({1});

// Measure matrix multiplication time at different numbers of input channels.
// Test with matrix data set to zeros and filled with random data.
// TODO(b/374695317): Optimise matrix multiplication to avoid multiplication by
//                    columns of zeros (inactive inputs).
static void BM_MatrixMultiplication(benchmark::State& state) {
  const size_t buffer_size = 256;
  const int number_of_input_channels = state.range(0);
  const int ambisonic_order = 7;
  const bool fill_with_random_data = state.range(1) == 1;

  // Create input buffer
  std::vector<float> input_buffer(number_of_input_channels * buffer_size, 0.0f);
  if (fill_with_random_data) {
    std::generate(input_buffer.begin(), input_buffer.end(), []() {
      return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    });
  } else {
    std::fill(input_buffer.begin(), input_buffer.end(), 0.0f);
  }

  // Create output buffer.
  std::vector<float> output_buffer(
      (ambisonic_order + 1) * (ambisonic_order + 1) * buffer_size, 0.0f);

  // Create an Ambisonic encoder object.
  AmbisonicEncoder encoder(buffer_size, number_of_input_channels,
                           ambisonic_order);

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

  // Assign sources to the encoder at all available input channels.
  for (int i = 0; i < number_of_input_channels; i++) {
    encoder.SetSource(i, 1.0f, directions[i].first, directions[i].second, 1.0f);
  }

  for (auto _ : state) {
    // Perform matrix multiplication.
    encoder.ProcessPlanarAudioData(input_buffer, output_buffer);
  }
}

// Setup benchmark
BENCHMARK(BM_MatrixMultiplication)
    ->Args({16, 0})
    ->Args({16, 1})
    ->Args({32, 0})
    ->Args({32, 1})
    ->Args({64, 0})
    ->Args({64, 1})
    ->Args({128, 0})
    ->Args({128, 1});

}  // namespace
}  // namespace iamf_tools
