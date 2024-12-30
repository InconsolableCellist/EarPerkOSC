#include "config.hpp"
#include <inih/INIReader.h>
#include <fstream>

Config::Config()
    : address("127.0.0.1")
    , port(9000)
    , address_left("/avatar/parameters/EarPerkLeft")
    , address_right("/avatar/parameters/EarPerkRight")
    , address_overwhelmingly_loud("/avatar/parameters/EarOverwhelm")
    , differential_threshold(0.01f)
    , volume_threshold(0.2f)
    , excessive_volume_threshold(0.5f)
    , reset_timeout_ms(1000)
    , timeout_ms(100)
{
}

bool Config::CreateDefaultConfigFile(const std::string& filename) {
    std::ofstream config_file(filename);
    if (!config_file.is_open()) {
        return false;
    }

    config_file << "[connection]\n"
        << "address=127.0.0.1\n"
        << "port=9000\n"
        << "osc_address_left=/avatar/parameters/EarPerkLeft\n"
        << "osc_address_right=/avatar/parameters/EarPerkRight\n"
        << "osc_address_overwhelmingly_loud=/avatar/parameters/EarOverwhelm\n\n"
        << "[audio]\n"
        << "differential_threshold=0.01\n"
        << "volume_threshold=0.2\n"
        << "excessive_volume_threshold=0.5\n"
        << "reset_timeout_ms=1000\n"
        << "timeout_ms=100\n";

    return true;
}

bool Config::LoadFromFile(const std::string& filename) {
    INIReader reader(filename);
    if (reader.ParseError() < 0) {
        if (!CreateDefaultConfigFile(filename)) {
            return false;
        }
        reader = INIReader(filename);
        if (reader.ParseError() < 0) {
            return false;
        }
    }

    address = reader.Get("connection", "address", address);
    port = reader.GetInteger("connection", "port", port);
    address_left = reader.Get("connection", "osc_address_left", address_left);
    address_right = reader.Get("connection", "osc_address_right", address_right);
    address_overwhelmingly_loud = reader.Get("connection", "osc_address_overwhelmingly_loud", address_overwhelmingly_loud);

    differential_threshold = reader.GetFloat("audio", "differential_threshold", differential_threshold);
    volume_threshold = reader.GetFloat("audio", "volume_threshold", volume_threshold);
    excessive_volume_threshold = reader.GetFloat("audio", "excessive_volume_threshold", excessive_volume_threshold);
    reset_timeout_ms = reader.GetInteger("audio", "reset_timeout_ms", reset_timeout_ms);
    timeout_ms = reader.GetInteger("audio", "timeout_ms", timeout_ms);

    return true;
}