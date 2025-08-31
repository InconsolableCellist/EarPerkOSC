#include "osc_sender.hpp"
#include "logger.hpp"
#include <stdexcept>
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

OSCSender::OSCSender(Config& config)
    : address(config.address)
    , port(config.port)
    , address_left(config.address_left)
    , address_right(config.address_right)
    , address_overwhelm(config.address_overwhelmingly_loud)
{
    LOG_DEBUG("OSCSender constructor called");
    LOG_DEBUG_F("OSC target: %s:%d", address.c_str(), port);
    LOG_DEBUG_F("OSC addresses - Left: %s, Right: %s, Overwhelm: %s", 
        address_left.c_str(), address_right.c_str(), address_overwhelm.c_str());
    
    // Initialize Winsock
    LOG_DEBUG("Initializing WinSock");
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG_ERROR_F("Failed to initialize WinSock: %d", result);
        throw std::runtime_error("Failed to initialize WinSock");
    }
    LOG_DEBUG("WinSock initialized successfully");
    LOG_INFO("OSCSender initialized successfully");
}

void OSCSender::SendLeftEar(bool value) {
    SendOSCMessage(address_left, value);
}

void OSCSender::SendRightEar(bool value) {
    SendOSCMessage(address_right, value);
}

void OSCSender::SendOverwhelm(bool value) {
    SendOSCMessage(address_overwhelm, value);
}

void OSCSender::SendOSCMessage(const std::string& addr, bool value) {
    try {
        // Create the socket
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            LOG_ERROR_F("Failed to create socket: %d", WSAGetLastError());
            throw std::runtime_error("Failed to create socket");
        }

        // Create OSC packet
        OSCPP::Client::Packet packet(buffer.data(), buffer.size());
        packet.openMessage(addr.c_str(), 1)
            .int32(value ? 1 : 0)
            .closeMessage();

        // Set up the address structure
        sockaddr_in destAddr;
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &(destAddr.sin_addr));

        // Send the packet
        int result = sendto(sock, buffer.data(), packet.size(), 0,
            reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));
        
        if (result == SOCKET_ERROR) {
            LOG_ERROR_F("Failed to send OSC message to %s: %d", addr.c_str(), WSAGetLastError());
        } else {
            LOG_DEBUG_F("Sent OSC message: %s = %s", addr.c_str(), value ? "true" : "false");
        }

        closesocket(sock);
    }
    catch (const std::exception& e) {
        LOG_ERROR_F("Exception in SendOSCMessage: %s", e.what());
    }
}