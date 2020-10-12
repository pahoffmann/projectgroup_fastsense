/**
 * @file communication.cpp
 * @author Marcel Flottmann
 * @date 2020-10-6
 */

#include "catch2_config.h"
#include <comm/receiver.h>
#include <comm/sender.h>
#include <msg/point_cloud.h>
#include <msg/tsdf_bridge_msg.h>
#include <iostream>
#include <thread>

using namespace fastsense::comm;
using namespace fastsense::msg;

TEST_CASE("Simple Sender Receiver Test", "[communication]")
{
    std::cout << "Testing 'Simple Sender Receiver Test'" << std::endl;
    int value_received;
    int value_to_send = 42;

    std::thread receive_thread{[&]()
    {
        Receiver<int> receiver{"127.0.0.1", 1234};
        std::this_thread::sleep_for(std::chrono::seconds(2));
        value_received = receiver.receive();
    }};

    std::thread send_thread{[&]()
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Sender<int> sender{1234};
        std::this_thread::sleep_for(std::chrono::seconds(2));
        sender.send(value_to_send);
    }};

    receive_thread.join();
    send_thread.join();
    REQUIRE(value_to_send == value_received);
}

TEST_CASE("PointCloud Sender Receiver Test", "[communication]")
{
    std::cout << "Testing 'PointCloud Sender Receiver Test'" << std::endl;
    PointCloud pc_to_send;
    pc_to_send.rings_ = 2;
    pc_to_send.points_.push_back({1, 2, 3});
    pc_to_send.points_.push_back({2, 3, 4});
    pc_to_send.points_.push_back({3, 4, 5});
    PointCloud pc_received;

    std::thread receive_thread{[&]()
    {
        Receiver<PointCloud> receiver{"127.0.0.1", 1234};
        std::this_thread::sleep_for(std::chrono::seconds(2));
        receiver.receive(pc_received);
    }};

    std::thread send_thread{[&]()
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Sender<PointCloud> sender{1234};
        std::this_thread::sleep_for(std::chrono::seconds(2));
        sender.send(pc_to_send);
    }};

    receive_thread.join();
    send_thread.join();
    REQUIRE(pc_to_send.rings_ == pc_received.rings_);
    REQUIRE(pc_to_send.points_ == pc_received.points_);
}

TEST_CASE("TSDFBridgeMessage Sender Receiver Test", "[communication]")
{
    std::cout << "Testing 'TSDFBridgeMessage Sender Receiver Test'" << std::endl;
    TSDFBridgeMessage tsdf_msg;
    tsdf_msg.tau_ = 2;
    tsdf_msg.map_resolution_ = 1;
    tsdf_msg.size_ = {10, 10, 10};
    tsdf_msg.pos_ = {0, 0, 0};
    tsdf_msg.offset_ = {0, 0, 0};
    tsdf_msg.tsdf_data_.resize(10 * 10 * 10);
    for (size_t i = 0; i < 10 * 10 * 10; ++i)
    {
        tsdf_msg.tsdf_data_[i].first = 0;
        tsdf_msg.tsdf_data_[i].second = 0;
    }

    tsdf_msg.tsdf_data_[4 + 5 * 10 + 5 * 10 * 10].first = 1;
    tsdf_msg.tsdf_data_[4 + 5 * 10 + 5 * 10 * 10].second = 1;

    tsdf_msg.tsdf_data_[3 + 5 * 10 + 5 * 10 * 10].first = 2;
    tsdf_msg.tsdf_data_[3 + 5 * 10 + 5 * 10 * 10].second = 1;

    tsdf_msg.tsdf_data_[6 + 5 * 10 + 5 * 10 * 10].first = -1;
    tsdf_msg.tsdf_data_[6 + 5 * 10 + 5 * 10 * 10].second = 1;

    tsdf_msg.tsdf_data_[7 + 5 * 10 + 5 * 10 * 10].first = -2;
    tsdf_msg.tsdf_data_[7 + 5 * 10 + 5 * 10 * 10].second = 1;

    TSDFBridgeMessage tsdf_received;

    std::thread receive_thread{[&]()
    {
        Receiver<TSDFBridgeMessage> receiver{"127.0.0.1", 1234};
        std::this_thread::sleep_for(std::chrono::seconds(2));
        receiver.receive(tsdf_received);
    }};

    std::thread send_thread{[&]()
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Sender<TSDFBridgeMessage> sender{1234};
        std::this_thread::sleep_for(std::chrono::seconds(2));
        sender.send(tsdf_msg);
    }};

    receive_thread.join();
    send_thread.join();
    REQUIRE(tsdf_msg.tau_ == tsdf_received.tau_);
    REQUIRE(tsdf_msg.map_resolution_ == tsdf_received.map_resolution_);
    REQUIRE(tsdf_msg.size_ == tsdf_received.size_);
    REQUIRE(tsdf_msg.pos_ == tsdf_received.pos_);
    REQUIRE(tsdf_msg.offset_ == tsdf_received.offset_);
    REQUIRE(tsdf_msg.tsdf_data_ == tsdf_received.tsdf_data_);
}