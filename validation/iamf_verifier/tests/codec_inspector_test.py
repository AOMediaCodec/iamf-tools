# Copyright (c) 2026, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.
"""Unit tests for codec_inspector."""

import io

from absl.testing import absltest
from absl.testing import parameterized
from validation.iamf_verifier import codec_inspector


class IsLossyBitstreamTest(parameterized.TestCase):
  """Tests for `codec_inspector.is_lossy_bitstream`.

  The test bitstreams are snippets, not valid IAMF bitstreams.
  `codec_inspector` is a partial parser that only reads OBU headers and the
  first two fields of the Codec Config OBU, so each snippet has just those
  bytes. Most leave out the IA Sequence Header OBU, and each Codec Config OBU
  ends right after `codec_id`.

  The first byte of an OBU header is `obu_type << 3`, plus 0x04 for
  `obu_redundant_copy`, 0x02 for `obu_trimming_status_flag`, and 0x01 for
  `obu_extension_flag`.
  """

  @parameterized.named_parameters(
      dict(
          testcase_name="opus_is_lossy",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x05"  # obu_size=5.
              b"\x00"  # codec_config_id=0.
              b"Opus"  # codec_id.
          ),
          expected_lossy=True,
      ),
      dict(
          testcase_name="aac_is_lossy",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x05"  # obu_size=5.
              b"\x00"  # codec_config_id=0.
              b"mp4a"  # codec_id.
          ),
          expected_lossy=True,
      ),
      dict(
          testcase_name="flac_is_lossless",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x05"  # obu_size=5.
              b"\x00"  # codec_config_id=0.
              b"fLaC"  # codec_id.
          ),
          expected_lossy=False,
      ),
      dict(
          testcase_name="lpcm_is_lossless",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x05"  # obu_size=5.
              b"\x00"  # codec_config_id=0.
              b"ipcm"  # codec_id.
          ),
          expected_lossy=False,
      ),
      dict(
          testcase_name="skips_codec_config_extension_header",
          # IAMF defines no extension headers yet, but parsers SHOULD ignore
          # them.
          bitstream=(
              b"\x01"  # obu_type=0 (Codec Config), obu_extension_flag=1.
              b"\x08"  # obu_size=8.
              b"\x02"  # extension_header_size=2.
              b"\xaa\xbb"  # Arbitrary extension_header_bytes.
              b"\x00"  # codec_config_id=0.
              b"Opus"  # codec_id.
          ),
          expected_lossy=True,
      ),
      dict(
          testcase_name="skips_ia_sequence_header",
          # Real bitstreams have an IA Sequence Header OBU before the Codec
          # Config OBU.
          bitstream=(
              b"\xf8"  # obu_type=31 (IA Sequence Header), no flags.
              b"\x06"  # obu_size=6.
              b"iamf"  # ia_code.
              b"\x00"  # primary_profile=0 (Simple).
              b"\x00"  # additional_profile=0 (Simple).
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x05"  # obu_size=5.
              b"\x00"  # codec_config_id=0.
              b"fLaC"  # codec_id.
          ),
          expected_lossy=False,
      ),
      dict(
          testcase_name="stops_at_codec_config",
          # IAMF v1.1 allows only one Codec Config OBU, so parsing stops there
          # and the truncated OBU after it is never read.
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x05"  # obu_size=5.
              b"\x00"  # codec_config_id=0.
              b"Opus"  # codec_id.
              b"\x08"  # obu_type=1 (Audio Element), no flags.
              b"\x05"  # obu_size=5, but no payload follows.
          ),
          expected_lossy=True,
      ),
  )
  def test_is_lossy_bitstream_returns_expected_lossiness(
      self, bitstream: bytes, expected_lossy: bool
  ):
    result = codec_inspector.is_lossy_bitstream(io.BytesIO(bitstream))

    self.assertEqual(result, expected_lossy)

  @parameterized.named_parameters(
      dict(
          testcase_name="empty_bitstream",
          bitstream=b"",
          error_regex="No Codec Config OBU found",
      ),
      dict(
          testcase_name="ia_sequence_header_only",
          bitstream=(
              b"\xf8"  # obu_type=31 (IA Sequence Header), no flags.
              b"\x06"  # obu_size=6.
              b"iamf"  # ia_code.
              b"\x00"  # primary_profile=0 (Simple).
              b"\x00"  # additional_profile=0 (Simple).
          ),
          error_regex="No Codec Config OBU found",
      ),
      dict(
          testcase_name="temporal_delimiter_before_codec_config",
          bitstream=(
              b"\x20"  # obu_type=4 (Temporal Delimiter), no flags.
              b"\x00"  # obu_size=0.
              # A Codec Config OBU after a Temporal Unit OBU is never read.
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x05"  # obu_size=5.
              b"\x00"  # codec_config_id=0.
              b"Opus"  # codec_id.
          ),
          error_regex="No Codec Config OBU found",
      ),
      dict(
          testcase_name="truncated_codec_config",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x05"  # obu_size=5, but only 3 bytes follow.
              b"\x00"  # codec_config_id=0.
              b"Op"  # The first 2 bytes of codec_id.
          ),
          error_regex="Truncated OBU",
      ),
      dict(
          testcase_name="empty_codec_config",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x00"  # obu_size=0, so codec_config_id is missing.
          ),
          error_regex="Truncated LEB128",
      ),
      dict(
          testcase_name="obu_size_too_small_for_codec_id",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x03"  # obu_size=3 ends the OBU after "Op".
              b"\x00"  # codec_config_id=0.
              b"Opus"  # codec_id, cut to "Op" by obu_size.
          ),
          error_regex="Unknown codec_id b'Op'",
      ),
      dict(
          testcase_name="unknown_codec_id",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x05"  # obu_size=5.
              b"\x00"  # codec_config_id=0.
              b"abcd"  # codec_id not defined by IAMF.
          ),
          error_regex="Unknown codec_id b'abcd'",
      ),
      dict(
          testcase_name="obu_size_exceeds_2_mb",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x81\x80\x80\x01"  # obu_size=2^21 + 1, over 2 MB.
          ),
          error_regex="OBU size 2097153 exceeds maximum of 2097152 bytes",
      ),
      dict(
          testcase_name="obu_size_leb128_exceeds_8_bytes",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x80\x80\x80\x80\x80\x80\x80\x80\x00"  # obu_size in 9 bytes.
          ),
          error_regex="LEB128 encoding exceeds 8 bytes",
      ),
      dict(
          testcase_name="codec_config_id_leb128_exceeds_32_bits",
          bitstream=(
              b"\x00"  # obu_type=0 (Codec Config), no flags.
              b"\x09"  # obu_size=9.
              b"\xff\xff\xff\xff\x7f"  # codec_config_id=2^35 - 1, over 32 bits.
              b"Opus"  # codec_id.
          ),
          error_regex="LEB128 value .* exceeds 32 bits",
      ),
  )
  def test_is_lossy_bitstream_invalid_bitstream_raises_value_error(
      self, bitstream: bytes, error_regex: str
  ):
    with self.assertRaisesRegex(ValueError, error_regex):
      codec_inspector.is_lossy_bitstream(io.BytesIO(bitstream))


if __name__ == "__main__":
  absltest.main()
