/**
 * @author Patrick Hoffmann
 * @author Malte Hillmann
 * @author Marc Eisoldt
 */

#include <util/point.h>
#include <registration/registration.h>

using namespace fastsense;
using namespace fastsense::registration;

Vector3i transform(const Vector3i& input, const Matrix4i& mat)
{
    return (mat.block<3, 3>(0, 0) * input + mat.block<3, 1>(0, 3)) / MATRIX_RESOLUTION;
}

void Registration::transform_point_cloud(fastsense::ScanPoints_t& in_cloud, const Matrix4f& mat)
{
    #pragma omp parallel for schedule(static)
    for (auto index = 0u; index < in_cloud.size(); ++index)
    {
        auto& point = in_cloud[index];
        Vector3f tmp = (mat.block<3, 3>(0, 0) * point.cast<float>() + mat.block<3, 1>(0, 3));
        tmp[0] < 0 ? tmp[0] -= 0.5 : tmp[0] += 0.5;
        tmp[1] < 0 ? tmp[1] -= 0.5 : tmp[1] += 0.5;
        tmp[2] < 0 ? tmp[2] -= 0.5 : tmp[2] += 0.5;

        point = tmp.cast<int>();
    }
}

void Registration::transform_point_cloud(fastsense::buffer::InputBuffer<PointHW>& in_cloud, const Matrix4f& mat)
{
    // TODO: test if this is faster on HW
    #pragma omp parallel for schedule(static)
    for (auto index = 0u; index < in_cloud.size(); ++index)
    {
        auto& point = in_cloud[index];
        Vector3f eigen_point(point.x, point.y, point.z);
        Vector3f tmp = (mat.block<3, 3>(0, 0) * eigen_point + mat.block<3, 1>(0, 3));
        tmp[0] < 0 ? tmp[0] -= 0.5 : tmp[0] += 0.5;
        tmp[1] < 0 ? tmp[1] -= 0.5 : tmp[1] += 0.5;
        tmp[2] < 0 ? tmp[2] -= 0.5 : tmp[2] += 0.5;

        point.x = static_cast<int>(tmp.x());
        point.y = static_cast<int>(tmp.y());
        point.z = static_cast<int>(tmp.z());
    }
}


Matrix4f Registration::xi_to_transform(Vector6f xi)
{
    // Formula 3.9 on Page 40 of "Truncated Signed Distance Fields Applied To Robotics"

    // Rotation around an Axis.
    // Direction of axis (l) = Direction of angular_velocity
    // Angle of Rotation (theta) = Length of angular_velocity
    auto angular_velocity = xi.block<3, 1>(0, 0);
    auto theta = angular_velocity.norm();
    auto l = angular_velocity / theta;
    Eigen::Matrix3f L;
    L <<
      0, -l.z(), l.y(),
      l.z(), 0, -l.x(),
      -l.y(), l.x(), 0;

    Matrix4f transform = Matrix4f::Identity();

    auto rotation = transform.block<3, 3>(0, 0);
    rotation += sin(theta) * L + (1 - cos(theta)) * L * L;

    transform.block<3, 1>(0, 3) = xi.block<3, 1>(3, 0);
    return transform;
}

Matrix4f Registration::register_cloud(fastsense::map::LocalMap& localmap, fastsense::buffer::InputBuffer<PointHW>& cloud)
{
    mutex_.lock();
    Matrix4f total_transform = imu_accumulator_; //transform used to register the pcl
    imu_accumulator_.setIdentity();
    mutex_.unlock();

    krnl.synchronized_run(localmap, cloud, max_iterations_, it_weight_gradient_, epsilon_, total_transform);

    // apply final transformation
    transform_point_cloud(cloud, total_transform);

    return total_transform;
}

/**
 * @brief Gets angluar velocity data from the IMU and stores them in the global_transform object
 *
 * @param imu ROS Message containing the necessary data
 * TODO: auslagern in andere Klasse
 * TODO: determine weather the queue of the pcl callback might be a probl.
 */
void Registration::update_imu_data(const fastsense::msg::ImuStamped& imu)
{
    if (first_imu_msg_ == true)
    {
        imu_time_ = imu.second;
        first_imu_msg_ = false;
        return;
    }

    float acc_time = std::chrono::duration_cast<std::chrono::milliseconds>(imu.second.time - imu_time_.time).count() / 1000.0f;

    Vector3f ang_vel(imu.first.ang.x(), imu.first.ang.y(), imu.first.ang.z());

    Vector3f orientation = ang_vel * acc_time; //in radiants [rad, rad, rad]
    auto rotation =   Eigen::AngleAxisf(orientation.x(), Vector3f::UnitX())
                      * Eigen::AngleAxisf(orientation.y(), Vector3f::UnitY())
                      * Eigen::AngleAxisf(orientation.z(), Vector3f::UnitZ());

    Matrix4f local_transform = Matrix4f::Identity();
    local_transform.block<3, 3>(0, 0) = rotation.toRotationMatrix();

    mutex_.lock();
    imu_accumulator_ = local_transform * imu_accumulator_; //combine/update transforms
    mutex_.unlock();

    imu_time_ = imu.second;
}
