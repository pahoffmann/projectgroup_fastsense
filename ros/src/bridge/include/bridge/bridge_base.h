/**
 * @file bridge.h
 * @author Julian Gaal
 * @date 2020-09-29
 */

#pragma once

#include <string>
#include <ros/node_handle.h>
#include <comm/receiver.h>
#include <util/process_thread.h>

namespace fastsense::bridge
{   

template <typename T, typename P, int PORT>
class BridgeBase
{
private:
    comm::Receiver<T> receiver_;
    T msg_;
    ros::Publisher pub_;

public:
    BridgeBase() = delete;
    inline BridgeBase(ros::NodeHandle& n, std::string name, size_t msg_buffer_size = 1000) 
    :   receiver_{PORT}, 
        msg_{},
        pub_{} 
    {
        pub_ = n.advertise<P>(name, msg_buffer_size);
    }

    virtual ~BridgeBase() = default;
    
    virtual void run() 
    {
        if (auto _nbytes = receiver_.receive(msg_))
        {
            convert();
            publish();
        }
    }

    inline const T& msg() const 
    {
        return msg_;
    }

    inline ros::Publisher& pub()
    {
        return pub_;
    }

protected:
    virtual void convert() = 0;
    virtual void publish() = 0;
};
    
}