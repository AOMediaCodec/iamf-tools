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

#ifndef CLI_AMBISONIC_ENCODER_AMBISONIC_UTILS_H_
#define CLI_AMBISONIC_ENCODER_AMBISONIC_UTILS_H_

#include <cmath>
#include <numbers>

#include "absl/log/check.h"

// TODO(b/400635711): Use the one in the obr library once it is open-sourced.
// This code is forked from Resonance Audio's `misc_math.h`.
namespace iamf_tools {
// Defines conversion factor from degrees to radians.
inline constexpr float kRadiansFromDegrees =
    static_cast<float>(std::numbers::pi_v<float> / 180.0);

// Defines conversion factor from radians to degrees.
inline constexpr float kDegreesFromRadians =
    static_cast<float>(180.0 / std::numbers::pi_v<float>);

/*!\brief Returns the factorial (!) of x. If x < 0, it returns 0.
 *
 * \param x Input to take factorial of.
 * \return Computed factorial of input; 0 if the input is negative.
 */
inline float Factorial(int x) {
  if (x < 0) {
    return 0.0f;
  }
  float result = 1.0f;
  for (; x > 0; --x) {
    result *= static_cast<float>(x);
  }
  return result;
}

/*!\brief Returns the double factorial (!!) of x.
 *
 * For odd x:  1 * 3 * 5 * ... * (x - 2) * x.
 * For even x: 2 * 4 * 6 * ... * (x - 2) * x.
 * If x < 0, it returns 0.
 *
 * \param x Input to take double factorial of.
 * \return Computed double factorial of input; 0 if the input is negative.
 */
inline float DoubleFactorial(int x) {
  if (x < 0) {
    return 0.0f;
  }
  float result = 1.0f;
  for (; x > 0; x -= 2) {
    result *= static_cast<float>(x);
  }
  return result;
}

/*!\brief Computes `base`^`exp`, where `exp` is a *non-negative* integer.
 *
 * Computed using the squared exponentiation (a.k.a double-and-add) method.
 * When `T` is a floating point type, this has the same semantics as pow(), but
 * is much faster.
 * `T` can also be any integral type, in which case computations will be
 * performed in the value domain of this integral type, and overflow semantics
 * will be those of `T`.
 * You can also use any type for which `operator*=` is defined.

 * \param base Input to the exponent function. Any type for which *= is defined.
 * \param exp Integer exponent, must be greater than or equal to zero.
 * \return `base`^`exp`.
 */
template <typename T>
static inline T IntegerPow(T base, int exp) {
  DCHECK_GE(exp, 0);
  T result = static_cast<T>(1);
  while (true) {
    if (exp & 1) {
      result *= base;
    }
    exp >>= 1;
    if (!exp) break;
    base *= base;
  }
  return result;
}

/*!\brief Computes ACN channel sequence from a degree and order.
 *
 * \param degree Degree of the spherical harmonic.
 * \param order Order of the spherical harmonic.
 * \return Computed ACN channel sequence.
 */
inline int AcnSequence(int degree, int order) {
  DCHECK_GE(degree, 0);
  DCHECK_LE(-degree, order);
  DCHECK_LE(order, degree);

  return degree * degree + degree + order;
}

/*!\brief Computes normalization factor for Schmidt semi-normalized harmonics.
 *
 * The Schmidt semi-normalized spherical harmonics is used in AmbiX.
 *
 * \param degree Degree of the spherical harmonic.
 * \param order Order of the spherical harmonic.
 * \return Computed normalization factor.
 */
inline float Sn3dNormalization(int degree, int order) {
  DCHECK_GE(degree, 0);
  DCHECK_LE(-degree, order);
  DCHECK_LE(order, degree);
  return std::sqrt((2.0f - ((order == 0) ? 1.0f : 0.0f)) *
                   Factorial(degree - std::abs(order)) /
                   Factorial(degree + std::abs(order)));
}

}  // namespace iamf_tools

#endif  // CLI_AMBISONIC_ENCODER_AMBISONIC_UTILS_H_
