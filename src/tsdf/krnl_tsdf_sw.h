#pragma once

/**
 * @author Marc Eisoldt
 */

#include <map/local_map_hw.h>
#include <util/point_hw.h>
#include <hw/kernels/tsdf_kernel.h>

#include <utility>


namespace fastsense::tsdf
{

/**
 * @brief Software equivalent of the kernel TSDF function
 */
void krnl_tsdf_sw(PointHW* scanPoints0,
                  PointHW* scanPoints1,
                  int numPoints,
                  std::pair<int, int>* mapData0,
                  std::pair<int, int>* mapData1,
                  int sizeX,   int sizeY,   int sizeZ,
                  int posX,    int posY,    int posZ,
                  int offsetX, int offsetY, int offsetZ,
                  std::pair<int, int>* new_entries0,
                  std::pair<int, int>* new_entries1,
                  int tau,
                  int max_weight);

} // namespace fastsense::tsdf