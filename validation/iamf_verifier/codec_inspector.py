# Copyright (c) 2026, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 3-Clause Clear License
# and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
# License was not distributed with this source code in the LICENSE file, you
# can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
# Alliance for Open Media Patent License 1.0 was not distributed with this
# source code in the PATENTS file, you can obtain it at
# www.aomedia.org/license/patent.
"""Inspects the codecs used by IAMF bitstreams.

This is a partial parser, not a full IAMF parser. It reads OBU headers until it
finds the Codec Config OBU, then reads only `codec_config_id` and `codec_id`
from it. Everything else is skipped without being validated.
"""

import io
from typing import BinaryIO

from absl import logging


# OBU types defined by the IAMF specification.
_OBU_IA_CODEC_CONFIG = 0
_OBU_IA_PARAMETER_BLOCK = 3
_OBU_IA_AUDIO_FRAME_ID17 = 23

# Canonical 4CC codec identifiers defined by the IAMF specification.
_LOSSY_CODECS = frozenset({b"Opus", b"mp4a"})
_LOSSLESS_CODECS = frozenset({b"fLaC", b"ipcm"})

# IAMF limits a `leb128()` to 8 bytes and its decoded value to 32 bits.
_MAX_LEB128_SIZE = 8
_MAX_LEB128_VALUE = 0xFFFFFFFF

# All extant IAMF profiles limit OBU size to 2 MB.
_MAX_OBU_SIZE = 1 << 21


def _read_leb128(stream: BinaryIO) -> int:
  """Reads an unsigned LEB128 integer from a binary stream.

  Args:
    stream: Binary stream positioned at the start of the LEB128 value.

  Returns:
    The decoded value.

  Raises:
    ValueError: If the stream ends before the value is complete, or if the
      encoding exceeds the IAMF limits of 8 bytes or a 32-bit decoded value.
  """
  val = 0
  for i in range(_MAX_LEB128_SIZE):
    byte_data = stream.read(1)
    if not byte_data:
      raise ValueError("Truncated LEB128.")
    b = byte_data[0]
    val |= (b & 0x7F) << (7 * i)
    if (b & 0x80) == 0:
      if val > _MAX_LEB128_VALUE:
        raise ValueError(f"LEB128 value {val} exceeds 32 bits.")
      return val
  raise ValueError(f"LEB128 encoding exceeds {_MAX_LEB128_SIZE} bytes.")


def _read_codec_id(obu_payload: io.BytesIO) -> bytes:
  """Reads the `codec_id` of a Codec Config OBU.

  Args:
    obu_payload: Codec Config OBU payload, starting at `codec_config_id`.

  Returns:
    The 4CC `codec_id`.

  Raises:
    ValueError: If the payload is too small to hold `codec_id` or `codec_id` is
      not defined by the IAMF specification.
  """
  _read_leb128(obu_payload)  # codec_config_id
  codec_id = obu_payload.read(4)
  if codec_id not in _LOSSY_CODECS | _LOSSLESS_CODECS:
    raise ValueError(f"Unknown codec_id {codec_id!r}.")
  return codec_id


def is_lossy_bitstream(bitstream: BinaryIO) -> bool:
  """Checks whether the Codec Config OBU uses a lossy codec ('Opus', 'mp4a').

  Supports IAMF v1.1 only, which allows a single Codec Config OBU, so this
  stops at the first Codec Config OBU it finds.

  Args:
    bitstream: Binary stream containing the IAMF bitstream.

  Returns:
    True if the Codec Config OBU uses a lossy codec ('Opus', 'mp4a'); False if
    it uses a lossless codec ('fLaC', 'ipcm').

  Raises:
    ValueError: If the bitstream is malformed, uses an unknown codec, or
      contains no Codec Config OBU.
  """
  while True:
    header_byte = bitstream.read(1)
    if not header_byte:
      break
    # The first byte of the OBU header packs `obu_type` (5 bits),
    # `obu_redundant_copy`, `obu_trimming_status_flag`, and
    # `obu_extension_flag` (1 bit each).
    b = header_byte[0]
    obu_type = (b >> 3) & 0x1F
    obu_extension_flag = bool(b & 0x01)

    # All Descriptor OBUs must precede Temporal Unit OBUs (types 3 to 23).
    if _OBU_IA_PARAMETER_BLOCK <= obu_type <= _OBU_IA_AUDIO_FRAME_ID17:
      break

    obu_size = _read_leb128(bitstream)
    if obu_size > _MAX_OBU_SIZE:
      raise ValueError(
          f"OBU size {obu_size} exceeds maximum of {_MAX_OBU_SIZE} bytes."
      )
    obu = bitstream.read(obu_size)
    if len(obu) < obu_size:
      raise ValueError("Truncated OBU.")
    if obu_type != _OBU_IA_CODEC_CONFIG:
      continue

    # Finish parsing the OBU header. `obu_trimming_status_flag` must be 0 for
    # Codec Config OBUs, so only the extension header may be left.
    obu_reader = io.BytesIO(obu)
    if obu_extension_flag:
      # IAMF does not define any extension headers yet, and parsers SHOULD
      # ignore extension header bytes they do not understand.
      extension_header_size = _read_leb128(obu_reader)
      obu_reader.seek(extension_header_size, io.SEEK_CUR)

    codec_id = _read_codec_id(obu_reader)
    logging.info("Inspected bitstream: codec_id=%s", codec_id)
    return codec_id in _LOSSY_CODECS

  raise ValueError(
      "Invalid IAMF bitstream: No Codec Config OBU found before Temporal Units."
  )
