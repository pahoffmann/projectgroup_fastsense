/**
 * @file point_cloud.h
 * @author Marcel Flottmann
 * @date 2020-08-11
 */

#pragma once

#include <cstdint>
#include <vector>
#include <memory>

#include <msg/point.h>

namespace fastsense::msg
{

/**
 * @brief A point cloud with fixed rings
 *
 * All columns are stored sequentially. A column consists of a point from every ring.
 * So the data is stored as following (C3R4 = column 3 ring 4)
 * [ C1R1 C1R2 C1R3 C1R4 C2R1 C2R2 C2R3 C2R4 ... ]
 */
class PointCloud
{
public:
    using ptr = std::shared_ptr<PointCloud>;
    std::vector<Point> points;
    uint16_t rings;
};

} // namespace fastsense::msg;