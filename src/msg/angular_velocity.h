#pragma once

/**
 * @file angular_velocity.h
 * @author Julian Gaal
 */

#include <util/params.h>
#include <util/xyz_buffer.h>

constexpr double DEEGREES_TO_RADIANS = M_PI / 180.0;

namespace fastsense::msg
{

/**
 * @brief Represents angular velocity data from imu
 */
struct AngularVelocity : public util::XYZBuffer<double>
{
    AngularVelocity() = default;
    AngularVelocity(const double* angular_rate)
    {
        data_[0] = angular_rate[0] * DEEGREES_TO_RADIANS;
        data_[1] = angular_rate[1] * DEEGREES_TO_RADIANS;
        data_[2] = angular_rate[2] * DEEGREES_TO_RADIANS;
    }
};

} // namespace fastsense::msg
