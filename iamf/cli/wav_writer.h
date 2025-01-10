/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_WAV_WRITER_H_
#define CLI_WAV_WRITER_H_

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"

namespace iamf_tools {

class WavWriter {
 public:
  /*!\brief Factory function to create a `WavWriter`.
   *
   * Creates a `WavWriter` that can be used to write a wav file without knowing
   * the number of samples in advance.
   *
   * \param wav_filename Path of the file to write to.
   * \param num_channels Number of channels in the wav file, must be 1 or 2.
   * \param sample_rate_hz Sample rate of the wav file in Hz.
   * \param bit_depth Bit-depth of the wav file, must be 16, 24, or 32.
   * \param write_header If true, the wav header is written.
   * \return Unique pointer to `WavWriter` on success. `nullptr` otherwise.
   */
  static std::unique_ptr<WavWriter> Create(const std::string& wav_filename,
                                           int num_channels, int sample_rate_hz,
                                           int bit_depth,
                                           bool write_header = true);

  /*!\brief Finalizes the wav header and closes the underlying file.*/
  ~WavWriter();

  /*!\brief Returns the bit-depth.*/
  int bit_depth() const { return bit_depth_; }

  /*!\brief Writes samples to the wav file.
   *
   * There must be an integer number of samples and the number of samples %
   * `num_channels()` must equal 0. The number of samples is implicitly
   * calculated by `buffer.size()` / (bit_depth / 8).
   *
   * \param buffer Buffer of raw input PCM with channels interlaced and no
   *        padding.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status WriteSamples(const std::vector<uint8_t>& buffer);

  /*!\brief Aborts the write process and deletes the wav file.*/
  void Abort();

 private:
  typedef absl::AnyInvocable<int(FILE*, size_t, int, int)> WavHeaderWriter;

  /*!\brief Private Constructor. Used only by the factory function.
   *
   * \param filename_to_remove Path of the file; used to clean up the output
   *        file when aborting.
   * \param num_channels Number of channels in the wav file, must be 1 or 2.
   * \param sample_rate_hz Sample rate of the wav file in Hz.
   * \param bit_depth Bit-depth of the wav file, must be 16, 24, or 32.
   * \param file Pointer to the file to write to.
   * \param wav_header_writer Function that writes the header if non-empty.
   */
  WavWriter(const std::string& filename_to_remove, int num_channels,
            int sample_rate_hz, int bit_depth, FILE* file,
            WavHeaderWriter wav_header_writer);

  const size_t num_channels_;
  const size_t sample_rate_hz_;
  const size_t bit_depth_;
  size_t total_samples_written_;
  FILE* file_;
  const std::string filename_to_remove_;
  WavHeaderWriter wav_header_writer_;
};
}  // namespace iamf_tools

#endif  // CLI_WAV_WRITER_H_
