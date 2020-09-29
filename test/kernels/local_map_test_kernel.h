#pragma once

/**
 * @author Marcel Flottmann
 */

#include <hw/kernels/base_kernel.h>
#include <hw/buffer/buffer.h>
#include <map/local_map.h>
#include <iostream>

namespace fastsense::kernels
{

class LocalMapTestKernel : public BaseKernel
{
public:
    LocalMapTestKernel(const CommandQueuePtr& queue)
        : BaseKernel{queue, "krnl_local_map_test"}
    {}

    ~LocalMapTestKernel() = default;

    void run(map::LocalMap<std::pair<float, float>>& map)
    {
        resetNArg();
        setArg(map.getBuffer().getBuffer());
        auto m = map.getHardwareRepresentation();
        setArg(m.sizeX);
        setArg(m.sizeY);
        setArg(m.sizeZ);
        setArg(m.posX);
        setArg(m.posY);
        setArg(m.posZ);
        setArg(m.offsetX);
        setArg(m.offsetY);
        setArg(m.offsetZ);

        // Write buffers
        cmd_q_->enqueueMigrateMemObjects({map.getBuffer().getBuffer()}, CL_MIGRATE_MEM_OBJECT_DEVICE, nullptr, &pre_events_[0]);

        // Launch the Kernel
        cmd_q_->enqueueTask(kernel_, &pre_events_, &execute_events_[0]);

        // Read buffers
        cmd_q_->enqueueMigrateMemObjects({map.getBuffer().getBuffer()}, CL_MIGRATE_MEM_OBJECT_HOST, &execute_events_, &post_events_[0]);
    }
};

} // namespace fastsense::kernels