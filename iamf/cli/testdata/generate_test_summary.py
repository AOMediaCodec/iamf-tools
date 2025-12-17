"""Generates a summary of .textprotos."""

import collections
from collections.abc import Sequence
import csv
from glob import glob
import os
import re

from absl import app
from absl import flags
from absl import logging
from google.protobuf import text_format

from iamf.cli.proto import user_metadata_pb2


_TEST_REPOSITORY_TAG = flags.DEFINE_string(
    'test_repository_tag',
    'github/aomediacodec/libiamf/main',
    'Used to determine which test vectors to include in the summary or'
    ' explicitly set to empty string to include all test vectors.',
)

_TEST_DATA_DIR = flags.DEFINE_string(
    'test_data_dir',
    None,
    'Path to the directory containing test data files.',
    required=True,
)

_OUTPUT_DIR = flags.DEFINE_string(
    'output_dir',
    None,
    'Path to the output directory.',
    required=True,
)

SPEC_SECTIONS = (
    '3.1',
    '3.2',
    '3.3',
    '3.4',
    '3.5',
    '3.6',
    '3.6.1',
    '3.6.2',
    '3.6.2.1',
    '3.6.3',
    '3.7',
    '3.7.1',
    '3.7.2',
    '3.7.3',
    '3.7.4',
    '3.7.5',
    '3.7.6',
    '3.7.7',
    '3.8',
    '3.8.1',
    '3.8.2',
    '3.8.3',
    '3.9',
    '3.10',
    '3.11.1',
    '3.11.2',
    '3.11.3',
    '3.11.4',
    '4.1',
    '4.2',
    '5.1',
    '5.1.1',
    '5.1.2',
    '6.1',
    '6.2.1',
    '6.2.2',
    '6.2.3',
    '6.2.4',
    '7',
    '7.1',
    '7.2',
    '7.2.1',
    '7.2.2',
    '7.2.3',
    '7.3',
    '7.3.1',
    '7.3.2.1',
    '7.3.2.2',
    '7.3.2.3',
    '7.3.2.4',
    '7.3.3',
    '7.4',
    '7.5.1',
    '7.5.2',
    '7.6.1',
    '7.6.2',
    '9.1.2.1',
    '9.1.2.2',
    '9.1.2.3',
    '9.1.2.4',
)


def pretty_print(string) -> str:
  """Removes enclosing brackets and quotes in the printed string."""
  return re.sub(r'\[|\]|{|}|\'', '', str(string))


def main(argv: Sequence[str]) -> None:
  if len(argv) > 1:
    raise app.UsageError('Too many command-line arguments.')

  os.makedirs(_OUTPUT_DIR.value, exist_ok=True)

  test_repository_tag = _TEST_REPOSITORY_TAG.value
  test_proto_files = sorted(
      glob(os.path.join(_TEST_DATA_DIR.value, '*.textproto'))
  )
  summary = {}
  # spec_coverage is {'section': {'section_field': [test1, test2, ...]}}
  spec_coverage = collections.defaultdict(lambda: collections.defaultdict(list))
  for test_proto_file in test_proto_files:
    logging.info('Found %s', test_proto_file)
    with open(test_proto_file, 'r') as f:
      user_metadata_proto = text_format.ParseLines(
          f, user_metadata_pb2.UserMetadata()
      )
      # Filter out test vectors which do not have a matching tag, but allow all
      # when the tag is empty.
      if test_repository_tag and (
          test_repository_tag
          not in user_metadata_proto.test_vector_metadata.test_repository_tags
      ):
        continue
      prefix = user_metadata_proto.test_vector_metadata.file_name_prefix
      tested_sections = (
          user_metadata_proto.test_vector_metadata.primary_tested_spec_sections
      )
      summary[prefix] = {
          'description': (
              user_metadata_proto.test_vector_metadata.human_readable_description
          ),
          'base': user_metadata_proto.test_vector_metadata.base_test,
          'is_valid': user_metadata_proto.test_vector_metadata.is_valid,
          'is_valid_to_decode': (
              user_metadata_proto.test_vector_metadata.is_valid_to_decode
          ),
          'primary_tested_spec_sections': pretty_print(tested_sections),
      }
      for tested_section in tested_sections:
        # The sections are in the format "x.x" or "x.x/field".
        [section, _, field] = tested_section.partition('/')
        spec_coverage[section][field].append(prefix)

  # Generate CSV.
  with open(
      os.path.join(_OUTPUT_DIR.value, 'test_summary.csv'), 'w'
  ) as csv_file:
    writer = csv.writer(csv_file, quoting=csv.QUOTE_NONNUMERIC)
    writer.writerow([
        'test',
        'base',
        'primary_tested_spec_sections',
        'is_valid',
        'is_valid_to_decode',
        'description',
    ])
    for p in summary:
      writer.writerow([
          p,
          summary[p]['base'],
          summary[p]['primary_tested_spec_sections'],
          summary[p]['is_valid'],
          summary[p]['is_valid_to_decode'],
          summary[p]['description'],
      ])

  with open(os.path.join(_OUTPUT_DIR.value, 'coverage.csv'), 'w') as csv_file:
    writer = csv.writer(csv_file, quoting=csv.QUOTE_NONNUMERIC)
    writer.writerow([
        'primary_tested_spec_section',
        'primary_tested_spec_section_field',
        'tests',
    ])
    # Split the section label by '.' and sort by comparing each part
    # incrementally.
    sorted_sections = sorted(
        SPEC_SECTIONS,
        key=lambda x: [int(subsection) for subsection in x.split('.')],
    )
    for section in sorted_sections:
      if section in spec_coverage:
        # Sort by field alphabetically
        sorted_fields = sorted(spec_coverage[section])
        for field in sorted_fields:
          writer.writerow([
              section,
              field,
              pretty_print(sorted(spec_coverage[section][field])),
          ])
      else:
        writer.writerow([section, '', ''])


if __name__ == '__main__':
  app.run(main)
