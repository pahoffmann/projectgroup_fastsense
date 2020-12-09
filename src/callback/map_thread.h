#pragma once 

/**
 * @file map_thread.h
 * @author Steffen Hinderink, Marc Eisoldt
 */

#include <util/point_hw.h>
#include <msg/transform.h>
#include <map/local_map.h>
#include <eigen3/Eigen/Dense>
#include <util/process_thread.h>
#include <msg/tsdf_bridge_msg.h>
#include <hw/kernels/tsdf_kernel.h>
#include <util/config/config_manager.h>
#include <util/concurrent_ring_buffer.h>
#include <mutex>
#include <atomic>

namespace fastsense::callback
{

using Eigen::Vector3i;
using TSDFBuffer = util::ConcurrentRingBuffer<msg::TSDFBridgeMessage>;

/**
 * @brief This class encapsulates the asynchronous map shift, tsdf update and visualization of the map.
 */
class MapThread : public fastsense::util::ProcessThread
{
public:

    /**
     * @brief Construct a new MapThread object.
     * 
     * @param local_map Local map pointer of the cloud callback.
     *                  In this local map nothing is written, but it is copied into a "local" local map.
     * @param map_mutex Mutex for synchronisation between the map thread and the cloud callback for access to the local map.
     * @param period Parameter for the map thread.
     *               Maximum number of registration periods without a map shift and update. Ignored if lower than 1.
     * @param position_threshold Parameter for the map thread.
     *                           Distance from the current position to the last activated position at which the thread is activated (in meters).
     * @param tsdf_buffer Buffer for communication to the visualization thread.
     * @param q Program command queue.
     */
    MapThread(const std::shared_ptr<fastsense::map::LocalMap>& local_map, 
              std::mutex& map_mutex,
              std::shared_ptr<TSDFBuffer> tsdf_buffer,
              unsigned int period,
              float position_threshold,
              fastsense::CommandQueuePtr& q);

    /**
     * @brief Default destructor of the map thread.
     */
    ~MapThread() = default;

    /**
     * @brief Deleted copy constructor of the map thread.
     */
    MapThread(const MapThread&) = delete;
    
    /**
     * @brief Deleted assignment operator of the map thread.   
     */
    MapThread& operator=(const MapThread&) = delete;

    /**
     * @brief Starts the thread for shifting, updating and visualization the map
     *        if a specific number of registartion periods were performed 
     *        or the position of the system has changed by a predefined threshold.
     * 
     * @param pos Current position
     * @param points Current scan points
     */
    void go(const Vector3i& pos, const fastsense::buffer::InputBuffer<PointHW>& points);

    /**
     * @brief Stop the map thread safely.
     */
    virtual void stop() override;

protected:

    /**
     * @brief Shift, update and visualize the local map based on the current position and scanner data.
     *        The thread runs in an infinite loop.
     *        One iteration is started by the go function.
     *        The communication is done by a mutex that functions as a semaphore.
     */
    void thread_run() override;

private:

    /// Pointer to the local map
    const std::shared_ptr<fastsense::map::LocalMap> local_map_;
    /// Kernel object to perform an map update on hardware
    fastsense::kernels::TSDFKernel tsdf_krnl_;
    /// Mutex for synchronisation between the map thread and the cloud callback for access to the local map
    std::mutex& map_mutex_;
    /// Mutex functions as a semaphore to control the when the map thread starts
    std::mutex start_mutex_;
    /// Flag that indicates if the thread is active
    std::atomic<bool> active_;
    /// Position that is used for shifting, updating and visualization
    Vector3i pos_;
    /// Scan points that are used for shifting, updating and visualization
    std::unique_ptr<fastsense::buffer::InputBuffer<PointHW>> points_ptr_;
    /// Maximum number of registration periods without a map shift and update. Ignored if lower than 1
    unsigned int period_;
    /// Distance from the current position to the last activated position at which the thread is activated (in mm)
    float position_threshold_;
    /// Counter for the number of performed registrations after the last thread activation
    unsigned int reg_cnt_;
    /// Buffer for starting the map visualization thread
    std::shared_ptr<TSDFBuffer> tsdf_buffer_;
};

} // namespace fastsense::callback
