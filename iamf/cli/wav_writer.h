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
#include <string>
#include <vector>

namespace iamf_tools {

class WavWriter {
 public:
  // TODO(b/308410539): Move to a factory design pattern instead of a public
  //                    constructor that leaves a "limbo" instance for
  //                    unrecoverable errors.

  /*\!brief Initializes a WavWriter.
   *
   * Initializes a WavWriter that can be used to write a wav file without
   * knowing the number of samples in advance.
   *
   * \param wav_filename Path of the file to write to.
   * \param num_channels Number of channels in the wav file, must be 1 or 2.
   * \param sample_rate_hz Sample rate of the wav file in Hz.
   * \param bit_depth Bit-depth of the wav file, must be 16, 24, or 32.
   * \param write_header If true, the wav header is written.
   */
  WavWriter(const std::string& wav_filename, int num_channels,
            int sample_rate_hz, int bit_depth, bool write_header = true);

  /*\!brief Moves the `WavWriter` without closing the underlying file.*/
  WavWriter(WavWriter&& original);

  /*\!brief Finalizes the wav header and closes the underlying file.*/
  ~WavWriter();

  int bit_depth() const { return bit_depth_; }

  /*\!brief Writes samples to the wav file.
   *
   * There must be an integer number of samples and the number of samples %
   * `num_channels()` must equal 0. The number of samples is implicitly
   * calculated by `buffer.size()` / (bit_depth / 8).
   *
   * \param buffer Buffer of raw input PCM with channels interlaced and no
   *     padding.
   * \return `true` on success. `false` on failure.
   */
  bool WriteSamples(const std::vector<uint8_t>& buffer);

  /*\!brief Aborts the write process and deletes the wav file.*/
  void Abort();

 private:
  const size_t num_channels_;
  const size_t sample_rate_hz_;
  const size_t bit_depth_;
  const bool write_header_;
  size_t total_samples_written_;
  FILE* file_;
  const std::string filename_;
};
}  // namespace iamf_tools

#endif  // CLI_WAV_WRITER_H_
