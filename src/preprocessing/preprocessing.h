#pragma once

/**
 * @file preprocessing.h
 * @author Pascal Buschermöhle
 */

#include <util/point_hw.h>
#include <msg/point_cloud.h>
#include <hw/buffer/buffer.h>
#include <util/point.h>


namespace fastsense::preprocessing
{

class Preprocessing
{
public:
    void preprocess_scan(const fastsense::msg::PointCloudPtrStamped& cloud, fastsense::buffer::InputBuffer<PointHW>& scan_points);

    void reduction_filter(fastsense::msg::PointCloudPtrStamped& cloud);

    void median_filter(fastsense::msg::PointCloudPtrStamped& cloud, uint8_t window_size);

private:
    uint8_t median_from_array(std::vector<ScanPoint*> medians);
};

}