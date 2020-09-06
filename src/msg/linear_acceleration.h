/**
 * @file linear_acceleration.h
 * @author Julian Gaal
 * @date 2020-08-18
 */

#pragma once

#include "xyz_buffer.h"

namespace fastsense::msg
{

/**
 * @brief Represents linear acceleration data from imu
 */
struct LinearAcceleration : public XYZBuffer<double>
{
    LinearAcceleration() = default;
    explicit LinearAcceleration(const double* acceleration);
};

} // namespace fastsense::msg