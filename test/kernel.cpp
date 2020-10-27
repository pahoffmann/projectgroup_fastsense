/**
 * @author Marc Eisoldt
 */


#include <stdlib.h>
#include <fstream>
#include <iostream>

#include <hw/buffer/buffer.h>
#include <hw/kernels/vadd_kernel.h>
#include <hw/opencl.h>
#include <hw/fpga_manager.h>
#include <registration/registration.h>
#include <map/local_map.h>
#include <util/pcd/pcd_file.h>
#include <hw/kernels/tsdf_kernel.h>
#include <util/point_hw.h>

#include "catch2_config.h"

#include <eigen3/Eigen/Dense>


using fastsense::util::PCDFile;

namespace fastsense::registration
{

//static const int DATA_SIZE = 4096;

constexpr unsigned int SCALE = 1000;

constexpr float MAX_OFFSET = 100; // TODO: this is too much

// Test Translation
constexpr float TX = 0.3 * SCALE;
constexpr float TY = 0.3 * SCALE;
constexpr float TZ = 0.0 * SCALE;
// Test Rotation
constexpr float RY = 5 * (M_PI / 180); //radiants

constexpr float TAU = 1 * SCALE;
constexpr float MAX_WEIGHT = 10 * WEIGHT_RESOLUTION;

constexpr int MAX_ITERATIONS = 200;

constexpr int SIZE_X = 20 * SCALE / MAP_RESOLUTION;
constexpr int SIZE_Y = 20 * SCALE / MAP_RESOLUTION;
constexpr int SIZE_Z = 5 * SCALE / MAP_RESOLUTION; 

static void check_computed_transform(const ScanPoints_t& points_posttransform, ScanPoints_t& points_pretransform)
{
    int minimum = std::numeric_limits<int>::infinity();
    int maximum = -std::numeric_limits<int>::infinity();
    int average = 0;

    int average_x = 0;
    int average_y = 0;
    int average_z = 0;

    std::vector<int> dists(points_pretransform.size());

    for (size_t i = 0; i < points_pretransform.size(); i++)
    {
        Eigen::Vector3i sub = points_pretransform[i] - points_posttransform[i];
        auto norm = sub.norm();

        if(norm < minimum)
        {
            minimum = norm;
        }

        if(norm > maximum)
        {
            maximum = norm;
        }

        average += norm;
        average_x += std::abs(sub.x());
        average_y += std::abs(sub.y());
        average_z += std::abs(sub.z());

        dists[i] = norm;

        //std::cout << norm << std::endl;
        //REQUIRE(norm < MAX_OFFSET);
    }

    std::sort(dists.begin(), dists.end());
    
    std::cout << "minimum distance: " << minimum << std::endl;
    std::cout << "maximum distance: " << maximum << std::endl;
    std::cout << "average distance: " << average / points_pretransform.size() << std::endl;
    std::cout << "average distance x: " << average_x / points_pretransform.size() << std::endl;
    std::cout << "average distance y: " << average_y / points_pretransform.size() << std::endl;
    std::cout << "average distance z: " << average_z / points_pretransform.size() << std::endl;
    std::cout << "median distance: " << dists[dists.size() / 2 + 1] << std::endl;

    REQUIRE((average / points_pretransform.size()) < MAX_OFFSET);
}

std::shared_ptr<fastsense::buffer::InputBuffer<PointHW>> scan_points_to_input_buffer(ScanPoints_t& cloud, const fastsense::CommandQueuePtr q)
{
    auto buffer_ptr = std::make_shared<fastsense::buffer::InputBuffer<PointHW>>(q, cloud.size());
    for(size_t i = 0; i < cloud.size(); i++)
    {
        auto point = cloud[i];
        PointHW tmp(point.x(), point.y(), point.z());
        (*buffer_ptr)[i] = tmp;
    }
    return buffer_ptr;
}

static const std::string error_message =
    "Error: Result mismatch:\n"
    "i = %d CPU result = %d Device result = %d\n";

TEST_CASE("Kernel", "[kernel][slow]")
{
    std::cout << "Testing 'Kernel'" << std::endl;


    fastsense::CommandQueuePtr q = fastsense::hw::FPGAManager::create_command_queue();


    //test registration
    fastsense::registration::Registration reg(MAX_ITERATIONS);

    std::vector<std::vector<Vector3f>> float_points;
    unsigned int num_points;

    fastsense::util::PCDFile file("sim_cloud.pcd");
    file.readPoints(float_points, num_points);

    auto count = 0u;

    ScanPoints_t scan_points(num_points);

    auto q2 = fastsense::hw::FPGAManager::create_command_queue();
    fastsense::buffer::InputBuffer<PointHW> kernel_points(q2, num_points);

    for(const auto& ring : float_points)
    {
        for(const auto& point : ring)
        {
            scan_points[count].x() = point.x() * SCALE;
            scan_points[count].y() = point.y() * SCALE;
            scan_points[count].z() = point.z() * SCALE;

            kernel_points[count].x = scan_points[count].x();
            kernel_points[count].y = scan_points[count].y();
            kernel_points[count].z = scan_points[count].z();
            
            ++count;
        }
    }

    ScanPoints_t scan_points_2(scan_points);
    ScanPoints_t points_pretransformed_trans(scan_points);
    ScanPoints_t points_pretransformed_rot(scan_points);

    std::shared_ptr<fastsense::map::GlobalMap> global_map_ptr(new fastsense::map::GlobalMap("test_global_map.h5", 0.0, 0.0));

    fastsense::map::LocalMap local_map(SIZE_Y, SIZE_Y, SIZE_Z, global_map_ptr, q);

    // Initialize temporary testing variables

    Eigen::Matrix4f translation_mat;
    translation_mat << 1, 0, 0, TX,
                       0, 1, 0, TY,
                       0, 0, 1, TZ,
                       0, 0, 0,  1;

    Eigen::Matrix4f rotation_mat;
    rotation_mat <<  cos(RY), -sin(RY),      0, 0,
                     sin(RY),  cos(RY),      0, 0,
                     0,             0,       1, 0,
                     0,             0,       0, 1;

    //calc tsdf values for the points from the pcd and store them in the local map

    auto q3 = fastsense::hw::FPGAManager::create_command_queue();
    fastsense::kernels::TSDFKernel krnl(q3);

    krnl.run(local_map, kernel_points, TAU, MAX_WEIGHT);
    krnl.waitComplete();

    //fastsense::tsdf::update_tsdf(scan_points, Vector3i::Zero(), local_map, TAU, MAX_WEIGHT);

    SECTION("Test Registration Translation")
    {
        std::cout << "    Section 'Test Registration Translation'" << std::endl;

        reg.transform_point_cloud(points_pretransformed_trans, translation_mat);

        //copy from scanpoints to  inputbuffer
        auto buffer_ptr = scan_points_to_input_buffer(points_pretransformed_trans, q);
        auto& buffer = *buffer_ptr;
        auto result_matrix = reg.register_cloud(local_map, buffer, q);

        reg.transform_point_cloud(points_pretransformed_trans, result_matrix);
        check_computed_transform(points_pretransformed_trans, scan_points);

    }

    SECTION("Registration test Rotation")
    {
        std::cout << "    Section 'Registration test Rotation'" << std::endl;
        reg.transform_point_cloud(points_pretransformed_rot, rotation_mat);
        auto buffer_ptr = scan_points_to_input_buffer(points_pretransformed_rot, q);
        auto& buffer = *buffer_ptr;
        auto result_matrix = reg.register_cloud(local_map, buffer, q);

        reg.transform_point_cloud(points_pretransformed_rot, result_matrix);
        check_computed_transform(points_pretransformed_rot, scan_points_2);
    }
}

} //namespace fastsense::registration  