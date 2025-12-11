"""A script to run IAMF encoder on test vectors."""

import glob
import logging
import os
import re
import subprocess
import tempfile

from absl import app
from absl import flags
from google.protobuf import text_format

from iamf.cli.proto import test_vector_metadata_pb2
from iamf.cli.proto import user_metadata_pb2

_ENCODER_BIN = flags.DEFINE_string(
    'encoder_bin', None, 'Path to the encoder binary.', required=True
)
_TEST_DATA_DIR = flags.DEFINE_string(
    'test_data_dir',
    None,
    'Path to the directory containing test data files.',
    required=True,
)
_OUTPUT_DIR = flags.DEFINE_string(
    'output_dir', None, 'Path to the output directory.', required=True
)
_REGEX_FILTER = flags.DEFINE_string(
    'regex_filter',
    None,
    'Regex to filter test vector input files. The regex is searched for in'
    ' each filename.',
    required=False,
)


def _run_encoder(
    textproto_filename: str, user_metadata: user_metadata_pb2.UserMetadata
) -> None:
  """Runs the encoder on a single test vector."""
  logging.info(
      'Running encoder for %s.',
      textproto_filename,
  )

  with tempfile.NamedTemporaryFile(
      mode='w+b', suffix='.binpb'
  ) as temp_metadata_file:
    # It's easy for loudness to get out of sync in the test vectors. Create a
    # temp file with loudness validation disabled, and process it.
    user_metadata.test_vector_metadata.validate_user_loudness = False
    temp_metadata_file.write(user_metadata.SerializeToString())
    temp_metadata_file.flush()
    cmd = [
        _ENCODER_BIN.value,
        '--user_metadata_filename',
        temp_metadata_file.name,
        '--input_wav_directory',
        _TEST_DATA_DIR.value,
        '--output_iamf_directory',
        _OUTPUT_DIR.value,
    ]
    result = subprocess.run(
        cmd,
        check=False,
        capture_output=True,
        text=True,
    )
  if result.returncode != 0:
    logging.error(
        'Encoder failed for %s:\\nSTDOUT:%s\\nSTDERR:%s',
        textproto_filename,
        result.stdout,
        result.stderr,
    )


def main(_) -> None:
  logging.basicConfig(level=logging.INFO)

  os.makedirs(_OUTPUT_DIR.value, exist_ok=True)

  textproto_files = glob.glob(os.path.join(_TEST_DATA_DIR.value, '*.textproto'))
  if not textproto_files:
    logging.error('No textproto files found in %s', _TEST_DATA_DIR.value)
    return

  if _REGEX_FILTER.value:
    textproto_files = [
        f
        for f in textproto_files
        if re.search(_REGEX_FILTER.value, os.path.basename(f))
    ]

  for textproto_file in textproto_files:
    user_metadata = user_metadata_pb2.UserMetadata()
    with open(textproto_file, 'r') as f:
      text_format.Parse(f.read(), user_metadata)

    if user_metadata.test_vector_metadata.is_valid:
      _run_encoder(textproto_file, user_metadata)
    else:
      logging.info('Skipping %s because is_valid=false', textproto_file)
      continue


if __name__ == '__main__':
  app.run(main)
