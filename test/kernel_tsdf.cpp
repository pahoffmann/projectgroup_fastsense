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
#include <tsdf/krnl_tsdf_sw.h>

#include <tsdf/update_tsdf.h>

#include "catch2_config.h"

#include <eigen3/Eigen/Dense>


using fastsense::util::PCDFile;

namespace fastsense::tsdf
{

static void check_tsdf(const fastsense::buffer::InputOutputBuffer<std::pair<int ,int>>& tsdf_hw, const fastsense::buffer::InputOutputBuffer<std::pair<int, int>>& tsdf_sw)
{
    REQUIRE(tsdf_hw.size() == tsdf_sw.size());

    size_t err_count = 0;
    
    // size_t lower_count = 0;
    // size_t higher_count = 0;

    for(size_t i = 0; i < tsdf_hw.size(); ++i)
    {
        //REQUIRE(tsdf_hw[i].first == tsdf_sw[i].first);
    
        if(tsdf_hw[i].first != tsdf_sw[i].first)
        {
            ++err_count;
        }

        // if(abs(tsdf_hw[i].second) < abs(tsdf_sw[i].second))
        // {
        //     ++lower_count;
        // }

        // if(abs(tsdf_hw[i].second) > abs(tsdf_sw[i].second))
        // {
        //     ++higher_count;
        // }

        // if(tsdf_hw[i].first != tsdf_sw[i].first)
        // {
        //     std::cout << tsdf_hw[i].first << " " << tsdf_hw[i].second << "  :  " << tsdf_sw[i].first << " " << tsdf_sw[i].second << std::endl;
        // }
    }

    // std::cout << "lower: " << lower_count << std::endl;
    // std::cout << "higher: " << higher_count << std::endl;


    REQUIRE(err_count == 0);
}

TEST_CASE("Kernel_TSDF", "[kernel][slow]")
{
    std::cout << "Testing 'Kernel_TSDF'" << std::endl;

    fastsense::CommandQueuePtr q = fastsense::hw::FPGAManager::create_command_queue();
    fastsense::kernels::TSDFKernel krnl(q);

    SECTION("Simple_Scenario")
    {
        std::cout << "    Section 'Simple_Scenario'" << std::endl;

        constexpr int SCALE = MAP_RESOLUTION;

        constexpr int TAU = 3 * SCALE;

        constexpr int SIZE_X = 50 * SCALE / MAP_RESOLUTION;
        constexpr int SIZE_Y = 50 * SCALE / MAP_RESOLUTION;
        constexpr int SIZE_Z = 10 * SCALE / MAP_RESOLUTION;

        constexpr int WEIGHT_SCALE = SCALE / 8.0;

        SECTION("Generation")
        {
            std::cout << "        Section 'Generation'" << std::endl;

            std::shared_ptr<fastsense::map::GlobalMap> gm_ptr = std::make_shared<fastsense::map::GlobalMap>("MapTest.h5", 0, 0);
            fastsense::map::LocalMap localMap{SIZE_X, SIZE_Y, SIZE_Z, gm_ptr, q};

            Vector3i scanner_pos(0, 0, 0);

            // ScanPoints_t points(1);
            // points[0] = Vector3i(6, 0, 0) * SCALE + Vector3i::Constant(MAP_RESOLUTION / 2);

            // fastsense::tsdf::update_tsdf(points, scanner_pos, localMap, TAU, 100);

            fastsense::buffer::InputBuffer<PointHW> kernel_points(q, 1);

            kernel_points[0].x = 6 * SCALE + MAP_RESOLUTION / 2;
            kernel_points[0].y = MAP_RESOLUTION / 2;
            kernel_points[0].z = MAP_RESOLUTION / 2;

            krnl.run(localMap, kernel_points, TAU, 100);
            krnl.waitComplete();

            // Front values
            CHECK(localMap.value(6, 0, 0).first == 0);
            CHECK(localMap.value(5, 0, 0).first == 1 * SCALE);
            CHECK(localMap.value(4, 0, 0).first == 2 * SCALE);
            CHECK(localMap.value(3, 0, 0).first == TAU);
            CHECK(localMap.value(2, 0, 0).first == TAU);
            CHECK(localMap.value(1, 0, 0).first == TAU);

            // Front weights
            CHECK(localMap.value(6, 0, 0).second == 1 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(5, 0, 0).second == 1 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(4, 0, 0).second == 1 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(3, 0, 0).second == 1 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(2, 0, 0).second == 1 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(1, 0, 0).second == 1 * WEIGHT_RESOLUTION);

            // back values
            CHECK(localMap.value( 7, 0, 0).first == -1 * SCALE);
            CHECK(localMap.value( 8, 0, 0).first == -2 * SCALE);
            CHECK(localMap.value( 9, 0, 0).first ==  0 * SCALE);
            CHECK(localMap.value(10, 0, 0).first ==  0 * SCALE);
            CHECK(localMap.value(11, 0, 0).first ==  0 * SCALE);
            CHECK(localMap.value(12, 0, 0).first ==  0 * SCALE);

            // back weights
            CHECK((localMap.value( 7, 0, 0).second < 1 * WEIGHT_RESOLUTION && localMap.value( 7, 0, 0).second > 0));
            CHECK((localMap.value( 8, 0, 0).second < 1 * WEIGHT_RESOLUTION && localMap.value( 8, 0, 0).second > 0));
            CHECK(localMap.value( 9, 0, 0).second == 0);
            CHECK(localMap.value(10, 0, 0).second == 0);
            CHECK(localMap.value(11, 0, 0).second == 0);
            CHECK(localMap.value(12, 0, 0).second == 0);
        }

        SECTION("Update")
        {
            std::cout << "        Section 'Update'" << std::endl;

            std::shared_ptr<fastsense::map::GlobalMap> gm_ptr = std::make_shared<fastsense::map::GlobalMap>("MapTest.h5", 0, 7 * WEIGHT_RESOLUTION);
            fastsense::map::LocalMap localMap{SIZE_X, SIZE_Y, SIZE_Z, gm_ptr, q};

            Vector3i scanner_pos(0, 0, 0);
            
            // ScanPoints_t points(1);
            // points[0] = Vector3i(6, 0, 0) * SCALE + Vector3i::Constant(MAP_RESOLUTION / 2);

            // update_tsdf(points, scanner_pos, localMap, TAU, 100 * WEIGHT_RESOLUTION);

            fastsense::buffer::InputBuffer<PointHW> kernel_points(q, 1);

            kernel_points[0].x = 6 * SCALE + MAP_RESOLUTION / 2;
            kernel_points[0].y = MAP_RESOLUTION / 2;
            kernel_points[0].z = MAP_RESOLUTION / 2;

            krnl.run(localMap, kernel_points, TAU, 100 * WEIGHT_RESOLUTION);
            krnl.waitComplete();

            // Front values
            CHECK(localMap.value(6, 0, 0).first == 0);
            CHECK(localMap.value(5, 0, 0).first == 1 * WEIGHT_SCALE);
            CHECK(localMap.value(4, 0, 0).first == 2 * WEIGHT_SCALE);
            CHECK(localMap.value(3, 0, 0).first == 3 * WEIGHT_SCALE);
            CHECK(localMap.value(2, 0, 0).first == 3 * WEIGHT_SCALE);
            CHECK(localMap.value(1, 0, 0).first == 3 * WEIGHT_SCALE);

            // Front weights
            CHECK(localMap.value(6, 0, 0).second == 8 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(5, 0, 0).second == 8 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(4, 0, 0).second == 8 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(3, 0, 0).second == 8 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(2, 0, 0).second == 8 * WEIGHT_RESOLUTION);
            CHECK(localMap.value(1, 0, 0).second == 8 * WEIGHT_RESOLUTION);

            SECTION("TSDF Max Weight")
            {
                //update_tsdf(points, scanner_pos, localMap, TAU, WEIGHT_RESOLUTION);
                
                krnl.run(localMap, kernel_points, TAU, WEIGHT_RESOLUTION);
                krnl.waitComplete();

                CHECK(localMap.value(6, 0, 0).second == WEIGHT_RESOLUTION);
            }
            }
    }

    SECTION("Simulation_Cloud")
    {
        std::cout << "    Section 'Simple_Cloud'" << std::endl;

        constexpr unsigned int SCALE = 1000;
        constexpr float TAU = 1 * SCALE;
        constexpr float MAX_WEIGHT = 10 * WEIGHT_RESOLUTION;

        constexpr int SIZE_X = 20 * SCALE / MAP_RESOLUTION;
        constexpr int SIZE_Y = 20 * SCALE / MAP_RESOLUTION;
        constexpr int SIZE_Z = 5 * SCALE / MAP_RESOLUTION; 

        std::vector<std::vector<Vector3f>> float_points;
        unsigned int num_points;

        fastsense::util::PCDFile file("sim_cloud.pcd");
        file.readPoints(float_points, num_points);

        auto count = 0u;

        fastsense::buffer::InputBuffer<PointHW> kernel_points(q, num_points);

        std::vector<PointHW> kernel_points_sw(num_points);

        for(const auto& ring : float_points)
        {
            for(const auto& point : ring)
            {
                kernel_points[count].x = point.x() * SCALE;;
                kernel_points[count].y = point.y() * SCALE;;
                kernel_points[count].z = point.z() * SCALE;;

                kernel_points_sw[count].x = kernel_points[count].x;
                kernel_points_sw[count].y = kernel_points[count].y;
                kernel_points_sw[count].z = kernel_points[count].z;
                
                ++count;
            }
        }

        std::shared_ptr<fastsense::map::GlobalMap> global_map_ptr(new fastsense::map::GlobalMap("test_global_map.h5", 0.0, 0.0));
        fastsense::map::LocalMap local_map(SIZE_X, SIZE_Y, SIZE_Z, global_map_ptr, q);

        std::shared_ptr<fastsense::map::GlobalMap> global_map_sw_ptr(new fastsense::map::GlobalMap("test_global_map_sw.h5", 0.0, 0.0));
        fastsense::map::LocalMap local_map_sw(SIZE_X, SIZE_Y, SIZE_Z, global_map_sw_ptr, q);

        //calc tsdf values for the points from the pcd and store them in the local map

        krnl.run(local_map, kernel_points, TAU, MAX_WEIGHT);
        krnl.waitComplete();

        fastsense::buffer::InputOutputBuffer<std::pair<int, int>> new_entries(q, local_map_sw.get_size().x() * local_map_sw.get_size().y() * local_map_sw.get_size().z());       

        for(int i = 0; i < local_map.get_size().x() * local_map.get_size().y() * local_map.get_size().z(); ++i)
        {
            new_entries[i].first = 0;
            new_entries[i].second = 0;
        }

        fastsense::tsdf::krnl_tsdf_sw(kernel_points_sw.data(), 
                                    kernel_points_sw.data(),
                                    num_points,
                                    local_map_sw.getBuffer(),
                                    local_map_sw.getBuffer(),
                                    local_map_sw.get_size().x(), 
                                    local_map_sw.get_size().y(), 
                                    local_map_sw.get_size().z(),
                                    0, 
                                    0, 
                                    0,
                                    local_map_sw.get_offset().x(), 
                                    local_map_sw.get_offset().y(), 
                                    local_map_sw.get_offset().z(),
                                    new_entries,
                                    new_entries,
                                    TAU,
                                    MAX_WEIGHT);


        check_tsdf(local_map.getBuffer(), local_map_sw.getBuffer());
    }
}

} //namespace fastsense::tsdf