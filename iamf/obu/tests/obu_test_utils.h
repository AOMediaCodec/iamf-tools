/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef OBU_TESTS_OBU_TEST_UTILS_H_
#define OBU_TESTS_OBU_TEST_UTILS_H_

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Creates a full-order Mono AmbisonicsConfig for testing.
 *
 * \param order Ambisonics order (0, 1, 2, etc. max 14).
 * \return Mono-coded AmbisonicsConfig with a filled mapping.
 */
AmbisonicsConfig MakeFullOrderAmbisonicsMonoConfig(int order);

/*!\brief A mock OBU. */
class MockObu : public ObuBase {
 public:
  /*!\brief Constructor.
   *
   * \param header OBU header.
   * \param obu_type OBU type.
   */
  MockObu(const ObuHeader& header, ObuType obu_type)
      : ObuBase(header, obu_type) {}

  MOCK_METHOD(void, PrintObu, (), (const, override));

  MOCK_METHOD(absl::Status, ValidateAndWritePayload, (WriteBitBuffer & wb),
              (const, override));

  MOCK_METHOD(absl::Status, ReadAndValidatePayloadDerived,
              (int64_t payload_size, ReadBitBuffer& rb), (override));
};

/*!\brief A mock parameter definition. */
class MockParamDefinition : public ParamDefinition {
 public:
  MockParamDefinition()
      : ParamDefinition(ParamDefinition::kParameterDefinitionReservedEnd,
                        ParamDefinition::BaseArgs{}) {}

  explicit MockParamDefinition(const ParamDefinition::BaseArgs& args)
      : ParamDefinition(ParamDefinition::kParameterDefinitionReservedEnd,
                        args) {}

  MockParamDefinition(ParamDefinition::ParameterDefinitionType type,
                      const ParamDefinition::BaseArgs& args)
      : ParamDefinition(type, args) {}

  MOCK_METHOD(absl::Status, ValidateAndWrite, (WriteBitBuffer & wb),
              (const, override));
  MOCK_METHOD(absl::Status, ReadAndValidate, (ReadBitBuffer & rb), (override));

  MOCK_METHOD(std::unique_ptr<ParameterData>, CreateParameterData, (),
              (const, override));
  MOCK_METHOD(void, Print, (), (const, override));
};

/*!\brief Makes arguments for Mode 1 (`kModeScheduleInParameterBlock`).
 *
 * \param parameter_id The parameter ID.
 * \param parameter_rate The parameter rate.
 * \return `ParamDefinition::BaseArgs` configured for Mode 1
 *     (`kModeScheduleInParameterBlock`).
 */
ParamDefinition::BaseArgs MakeScheduleInParameterBlockBaseArgs(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate);

/*!\brief Makes arguments for a single subblock.
 *
 * \param parameter_id The parameter ID.
 * \param parameter_rate The parameter rate.
 * \param duration The duration of the parameter and the subblock.
 * \return `ParamDefinition::BaseArgs` configured for a single subblock.
 */
ParamDefinition::BaseArgs MakeOneSubblockParamDefinitionBaseArgs(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration);

/*!\brief Makes arguments for constant-duration subblocks.
 *
 * \param parameter_id The parameter ID.
 * \param parameter_rate The parameter rate.
 * \param duration The total duration.
 * \param constant_subblock_duration The constant subblock duration.
 * \param reserved The reserved field.
 * \return `ParamDefinition::BaseArgs` configured with the constant duration
 *     subblocks.
 */
ParamDefinition::BaseArgs MakeConstantSubblocksParamDefinitionBaseArgs(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration, DecodedUleb128 constant_subblock_duration);

/*!\brief Makes arguments for variable-duration subblocks.
 *
 * \param parameter_id The parameter ID.
 * \param parameter_rate The parameter rate.
 * \param subblock_durations The list of individual subblock durations.
 * \return BaseArgs configured with variable subblocks.
 */
ParamDefinition::BaseArgs MakeVariableSubblocksParamDefinitionBaseArgs(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    absl::Span<const DecodedUleb128> subblock_durations);

}  // namespace iamf_tools
#endif  // OBU_TESTS_OBU_TEST_UTILS_H_
