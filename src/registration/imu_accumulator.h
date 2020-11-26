#pragma once

/**
 * @file imu_accululator.h
 * @author Julian Gaal
 */

#include <util/time.h>
#include <mutex>
#include <util/point.h>
#include <msg/imu.h>

namespace fastsense::registration
{

class ImuAccumulator
{
public:
    ImuAccumulator();

    ~ImuAccumulator() = default;

    const Matrix4f& acc_transform() const;

    fastsense::Vector3f rot_in_euler() const;

    void reset();

    void update(const fastsense::msg::ImuStamped& imu);

    void update(const fastsense::msg::Imu& imu, double acc_time);

private:
    void apply_transform(double acc_time, const Vector3f& ang_vel);

    bool first_imu_msg_;
    Matrix4f acc_transform_;
    Matrix4f local_transform_;
    util::HighResTimePoint last_imu_timestamp_;
};

} // namespace fastsense::registration

std::ostream& operator<<(std::ostream& os, const fastsense::registration::ImuAccumulator& acc);