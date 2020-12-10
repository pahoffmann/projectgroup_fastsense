#pragma once

/**
 * @file transform.h
 * @author Marcel Flottmann
 */

#include <util/point.h>
#include <msg/stamped.h>
#include <util/concurrent_ring_buffer.h>
#include <msg/zmq_converter.h>

namespace fastsense::msg
{

/**
 * @brief Transform Message
 */
struct Transform : public ZMQConverter
{   
    /**
     * @brief Construct a new Transform object
     */
    Transform() 
    : rotation{Quaternionf::Identity()}
    , translation{Vector3f::Zero()}
    {}

    /**
     * @brief Construct a new Transform object
     * 
     * @param q Eigen::Quaternionf
     * @param t Eigen::Vector3f
     */
    Transform(Quaternionf q, Vector3f t)
    : rotation{q}
    , translation{t}
    {}

    /**
     * @brief QUATERNIONS ARE A FUCKING BITCH AND ARE INTERNALLY REORDERS WHEN BEING SENT/RECEIVED.
     * THIS IS UGLY AF BUT IT WORKS
     * @param msg to transform into transform
     */
    void from_zmq_msg(zmq::multipart_t &msg) override
    {
        auto trans = msg.pop();
        translation = *static_cast<Vector3f*>(trans.data());
        auto rot = msg.pop();
        float* data = (float*)rot.data();
        rotation.x() = data[0];
        rotation.y() = data[1];
        rotation.z() = data[2];
        rotation.w() = data[3];
    }

    /**
     * @brief QUATERNIONS ARE A FUCKING BITCH AND ARE INTERNALLY REORDERS WHEN BEING SENT/RECEIVED.
     * THIS IS UGLY AF BUT IT WORKS
     */
    zmq::multipart_t to_zmq_msg() const override
    {
        zmq::multipart_t multi;
        multi.add(zmq::message_t(translation.data(), 3 * sizeof(float)));
        float data[4] = {rotation.x(), rotation.y(), rotation.z(), rotation.w()};
        multi.add(zmq::message_t(data, 4 * sizeof(float)));
        return multi;
    }
    
    /// rotation
    Quaternionf rotation;

    /// translation
    Vector3f translation;
};

using TransformStamped = msg::Stamped<Transform>;
using TransformStampedBuffer = fastsense::util::ConcurrentRingBuffer<fastsense::msg::TransformStamped>;

} // namespace fastsense::msg