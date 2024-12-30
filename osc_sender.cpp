#include "osc_sender.hpp"
#include <stdexcept>
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

OSCSender::OSCSender(const Config& config)
    : address(config.address)
    , port(config.port)
    , address_left(config.address_left)
    , address_right(config.address_right)
    , address_overwhelm(config.address_overwhelmingly_loud)
{
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("Failed to initialize WinSock");
    }
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
        sendto(sock, buffer.data(), packet.size(), 0,
            reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));

        closesocket(sock);
    }
    catch (const std::exception& e) {
        // Log error or handle it appropriately
    }
}