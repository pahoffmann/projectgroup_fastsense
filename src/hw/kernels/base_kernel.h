#pragma once

/**
 * @author Julian Gaal
 * @author Marcel Flottmann
 */

#include <hw/opencl.h>
#include <hw/types.h>
#include <hw/fpga_manager.h>

namespace fastsense::kernels
{

class BaseKernel
{
private:
    int narg_;
protected:
    template <typename T>
    inline void setArg(const T& arg)
    {
        kernel_.setArg(narg_++, arg);
    }

    void resetNArg()
    {
        narg_ = 0;
    }

    cl::Kernel kernel_;
    fastsense::CommandQueuePtr cmd_q_;
    std::vector<cl::Event> pre_events_;
    std::vector<cl::Event> execute_events_;
    std::vector<cl::Event> post_events_;
public:
    inline BaseKernel(const fastsense::CommandQueuePtr& queue, const char* name)
        :   narg_{0},
            kernel_{fastsense::hw::FPGAManager::get_program(), name},
            cmd_q_{queue},
            pre_events_(1),
            execute_events_(1),
            post_events_(1)
    {}

    virtual ~BaseKernel() = default;

    virtual void waitComplete()
    {
        cl::Event::waitForEvents(post_events_);
    }
};

} // namespace fastsense::kernels