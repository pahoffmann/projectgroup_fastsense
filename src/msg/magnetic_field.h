/**
 * @file magnetic_field.h
 * @author Julian Gaal
 * @date 2020-08-18
 */

#pragma once

#include "xyz_buffer.h"

namespace fastsense::msg
{

/**
 * @brief Represents magnetic field data from imu
 */
struct MagneticField : public XYZBuffer<double>
{
    MagneticField() = default;
    explicit MagneticField(const double* magneticField);
};

} // namespace fastsense::msg