/**
 * @file update_tsdf_sw.cpp
 * @author Malte Hillmann
 * @author Marc Eisoldt
 */

#include "krnl_tsdf_sw.h"

#include <iostream>

#include <hls_stream.h>

using namespace fastsense::map;

constexpr int NUM_POINTS = 6000;
constexpr int SPLIT_FACTOR = 2;

namespace fastsense::tsdf
{

void read_points(PointHW* scanPoints,
                 int numPoints,
                 int tau,
                 PointHW map_pos,
                 hls::stream<PointHW>& point_fifo,
                 hls::stream<int>& distance_fifo,
                 hls::stream<int>& distance_tau_fifo,
                 hls::stream<PointHW>& direction_fifo,
                 hls::stream<PointHW>& abs_direction_fifo)
{
    for (int i = 0; i < numPoints; i++)
    {
        PointHW p = scanPoints[(i * 157) % numPoints];
        int dist = p.norm2();
        int dist_tau = dist + 2 * hls_sqrt_approx(dist) * tau + tau * tau; // (distance + tau)^2
        PointHW direction = p.to_map() - map_pos;
        PointHW abs_direction = direction.abs();
        point_fifo << p;
        distance_fifo << dist;
        distance_tau_fifo << dist_tau;
        direction_fifo << direction;
        abs_direction_fifo << abs_direction;
    }
}

void update_tsdf(int numPoints,
                 int sizeX,   int sizeY,   int sizeZ,
                 int posX,    int posY,    int posZ,
                 int offsetX, int offsetY, int offsetZ,
                 buffer::InputOutputBuffer<std::pair<int, int>>& new_entries,
                 int tau,
                 hls::stream<PointHW>& point_fifo,
                 hls::stream<int>& distance_fifo,
                 hls::stream<int>& distance_tau_fifo,
                 hls::stream<PointHW>& direction_fifo,
                 hls::stream<PointHW>& abs_direction_fifo)
{
    fastsense::map::LocalMapHW map{sizeX,   sizeY,   sizeZ,
                                   posX,    posY,    posZ,
                                   offsetX, offsetY, offsetZ};
    PointHW map_pos{posX, posY, posZ};
    PointHW map_offset{offsetX, offsetY, offsetZ};

    int weight_epsilon = tau / 10;

    // squared distances
    int distance;
    int distance_tau;
    int current_distance;

    PointHW current_point;
    PointHW current_cell;
    PointHW direction;
    PointHW direction2;
    PointHW increment;
    PointHW abs_direction;

    int err_1;
    int err_2;

    int interpolate_z;
    int interpolate_z_end;

    int point_idx = 0;

    bool load_new_point = true;

    while (point_idx < numPoints)
    {
        // Load point and init Bresenham
        if (load_new_point)
        {
            point_fifo >> current_point;
            distance_fifo >> distance;
            distance_tau_fifo >> distance_tau;
            direction_fifo >> direction;
            abs_direction_fifo >> abs_direction;

            current_distance = 0;
            current_cell = map_pos;

            increment = direction.sign();
            direction2 = abs_direction * 2;

            if ((abs_direction.x >= abs_direction.y) && (abs_direction.x >= abs_direction.z))
            {
                err_1 = direction2.y - abs_direction.x;
                err_2 = direction2.z - abs_direction.x;
            }
            else if ((abs_direction.y >= abs_direction.x) && (abs_direction.y >= abs_direction.z))
            {
                err_1 = direction2.x - abs_direction.y;
                err_2 = direction2.z - abs_direction.y;
            }
            else
            {
                err_1 = direction2.y - abs_direction.z;
                err_2 = direction2.x - abs_direction.z;
            }

            interpolate_z = current_cell.z;
            interpolate_z_end = interpolate_z;

            load_new_point = false;
        }

        // Calculate and update TSDF value
        int tsdf_value = (current_point - current_cell.to_mm()).norm();
        if (tsdf_value > tau)
        {
            tsdf_value = tau;
        }

        if (current_distance > distance)
        {
            tsdf_value = -tsdf_value;
        }

        int weight = WEIGHT_RESOLUTION;

        if (tsdf_value < -weight_epsilon)
        {
            weight = WEIGHT_RESOLUTION * (tau + tsdf_value) / (tau - weight_epsilon);
        }

        if (weight != 0 && map.in_bounds(current_cell.x, current_cell.y, interpolate_z))
        {
            int index = map.getIndex(current_cell.x, current_cell.y, interpolate_z);
            std::pair<int, int> entry = new_entries[index];

            if (entry.second == 0 || hls_abs(tsdf_value) < hls_abs(entry.first))
            {
                std::pair<int, int> new_entry;
                new_entry.first = tsdf_value;
                new_entry.second = weight;
                new_entries[index] = new_entry;
            }
        }

        // Update current_cell with Bresenham and interpolation
        if (interpolate_z == interpolate_z_end)
        {
            if (current_distance < distance_tau)
            {
                int approx_distance;
                if ((abs_direction.x >= abs_direction.y) && (abs_direction.x >= abs_direction.z))
                {
                    approx_distance = hls_abs(current_cell.x);
                    if (err_1 <= 0 && err_2 <= 0)
                    {
                        current_cell.x += increment.x;
                        err_1 += direction2.y;
                        err_2 += direction2.z;
                    }
                    else
                    {
                        if (err_1 > 0)
                        {
                            current_cell.y += increment.y;
                            err_1 -= direction2.x;
                        }
                        if (err_2 > 0)
                        {
                            current_cell.z += increment.z;
                            err_2 -= direction2.x;
                        }
                    }
                }
                else if ((abs_direction.y >= abs_direction.x) && (abs_direction.y >= abs_direction.z))
                {
                    approx_distance = hls_abs(current_cell.y);
                    if (err_1 <= 0 && err_2 <= 0)
                    {
                        current_cell.y += increment.y;
                        err_1 += direction2.x;
                        err_2 += direction2.z;
                    }
                    else
                    {
                        if (err_1 > 0)
                        {
                            current_cell.x += increment.x;
                            err_1 -= direction2.y;
                        }
                        if (err_2 > 0)
                        {
                            current_cell.z += increment.z;
                            err_2 -= direction2.y;
                        }
                    }
                }
                else
                {
                    approx_distance = hls_abs(current_cell.z);
                    if (err_1 <= 0 && err_2 <= 0)
                    {
                        current_cell.z += increment.z;
                        err_1 += direction2.y;
                        err_2 += direction2.x;
                    }
                    else
                    {
                        if (err_1 > 0)
                        {
                            current_cell.y += increment.y;
                            err_1 -= direction2.z;
                        }
                        if (err_2 > 0)
                        {
                            current_cell.x += increment.x;
                            err_2 -= direction2.z;
                        }
                    }
                }

                current_distance = (current_cell - map_pos).to_mm().norm2();
                int delta_z = dz_per_distance * approx_distance / MATRIX_RESOLUTION;
                interpolate_z = (current_cell.z * MAP_RESOLUTION - delta_z) / MAP_RESOLUTION;
                interpolate_z_end = (current_cell.z * MAP_RESOLUTION + delta_z) / MAP_RESOLUTION;
            }
            else
            {
                point_idx++;
                load_new_point = true;
            }
        }
        else
        {
            interpolate_z++;
        }
    }
}

void tsdf_dataflow(PointHW* scanPoints,
                   int numPoints,
                   int sizeX,   int sizeY,   int sizeZ,
                   int posX,    int posY,    int posZ,
                   int offsetX, int offsetY, int offsetZ,
                   buffer::InputOutputBuffer<std::pair<int, int>>& new_entries,
                   int tau)
{
    hls::stream<PointHW> point_fifo;
    hls::stream<int> distance_fifo;
    hls::stream<int> distance_tau_fifo;
    hls::stream<PointHW> direction_fifo;
    hls::stream<PointHW> abs_direction_fifo;

    read_points(scanPoints, numPoints, tau, PointHW{posX, posY, posZ}, point_fifo, distance_fifo, distance_tau_fifo, direction_fifo, abs_direction_fifo);
    update_tsdf(numPoints,
                sizeX,   sizeY,   sizeZ,
                posX,    posY,    posZ,
                offsetX, offsetY, offsetZ,
                new_entries, tau,
                point_fifo, distance_fifo, distance_tau_fifo, direction_fifo, abs_direction_fifo);
}


void sync_loop(
    buffer::InputOutputBuffer<std::pair<int, int>>& mapData,
    int start,
    int end,
    buffer::InputOutputBuffer<std::pair<int, int>>& new_entries,
    int max_weight)
{
    for (int index = start; index < end; index++)
    {
        std::pair<int, int> map_entry = mapData[index];
        std::pair<int, int> new_entry = new_entries[index];

        int new_weight = map_entry.second + new_entry.second;

        if (!new_weight)
        {
            continue;
        }

        map_entry.first = (map_entry.first * map_entry.second + new_entry.first * new_entry.second) / new_weight;

        if (new_weight > max_weight)
        {
            new_weight = max_weight;
        }

        map_entry.second = new_weight;

        mapData[index] = map_entry;
    }
}

void tsdf_dataflower(
    PointHW* scanPoints0,
    PointHW* scanPoints1,
    int step,
    int last_step,
    buffer::InputOutputBuffer<std::pair<int, int>>& new_entries0,
    buffer::InputOutputBuffer<std::pair<int, int>>& new_entries1,
    int sizeX,   int sizeY,   int sizeZ,
    int posX,    int posY,    int posZ,
    int offsetX, int offsetY, int offsetZ,
    int tau)
{
    tsdf_dataflow(scanPoints0 + 0 * step, step,
                  sizeX,   sizeY,   sizeZ,
                  posX,    posY,    posZ,
                  offsetX, offsetY, offsetZ,
                  new_entries0, tau);

    tsdf_dataflow(scanPoints1 + 1 * step, last_step,
                  sizeX,   sizeY,   sizeZ,
                  posX,    posY,    posZ,
                  offsetX, offsetY, offsetZ,
                  new_entries1, tau);
}

void sync_looper(
    buffer::InputOutputBuffer<std::pair<int, int>>& mapData0,
    buffer::InputOutputBuffer<std::pair<int, int>>& mapData1,
    int step,
    int numPoints,
    buffer::InputOutputBuffer<std::pair<int, int>>& new_entries0,
    buffer::InputOutputBuffer<std::pair<int, int>>& new_entries1,
    int max_weight)
{
    sync_loop(mapData0, step * 0, step * (0 + 1), new_entries0, max_weight);
    sync_loop(mapData1, step * 1, numPoints, new_entries1, max_weight);
}

void krnl_tsdf_sw(PointHW* scanPoints0,
                  PointHW* scanPoints1,
                  int numPoints,
                  buffer::InputOutputBuffer<std::pair<int, int>>& mapData0,
                  buffer::InputOutputBuffer<std::pair<int, int>>& mapData1,
                  int sizeX,   int sizeY,   int sizeZ,
                  int posX,    int posY,    int posZ,
                  int offsetX, int offsetY, int offsetZ,
                  buffer::InputOutputBuffer<std::pair<int, int>>& new_entries0,
                  buffer::InputOutputBuffer<std::pair<int, int>>& new_entries1,
                  int tau,
                  int max_weight)
{
    int step = numPoints / SPLIT_FACTOR + 1;
    int last_step = numPoints - (SPLIT_FACTOR - 1) * step;
    tsdf_dataflower(scanPoints0,
                    scanPoints1,
                    step,
                    last_step,
                    new_entries0,
                    new_entries1,
                    sizeX,   sizeY,   sizeZ,
                    posX,    posY,    posZ,
                    offsetX, offsetY, offsetZ,
                    tau);


    int total_size = sizeX * sizeY * sizeZ;
    int sync_step = total_size / SPLIT_FACTOR + 1;

    sync_looper(mapData0,
                mapData1,
                sync_step,
                total_size,
                new_entries0,
                new_entries1,
                max_weight);

}

} // namespace fastsense::tsdf