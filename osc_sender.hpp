#pragma once
#include <string>
#include <array>
#include "oscpp/client.hpp"
#include "config.hpp"

class OSCSender {
public:
    explicit OSCSender(const Config& config);
    ~OSCSender() = default;

    // Delete copy constructor and assignment operator
    OSCSender(const OSCSender&) = delete;
    OSCSender& operator=(const OSCSender&) = delete;

    // Send boolean OSC messages
    void SendLeftEar(bool value);
    void SendRightEar(bool value);
    void SendOverwhelm(bool value);

private:
    void SendOSCMessage(const std::string& address, bool value);

    static const size_t MAX_PACKET_SIZE = 1024;
    std::array<char, MAX_PACKET_SIZE> buffer;

    std::string address;
    int port;
    std::string address_left;
    std::string address_right;
    std::string address_overwhelm;
};