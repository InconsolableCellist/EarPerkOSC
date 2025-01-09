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
    , auto_volume_threshold(false)
    , auto_excessive_threshold(false)
    , volume_threshold_multiplier(2.0f)  // 2 standard deviations above mean
    , excessive_threshold_multiplier(3.0f)  // 3 standard deviations above mean
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
        << "timeout_ms=100\n"
        << "auto_volume_threshold=false\n"
        << "auto_excessive_threshold=false\n"
        << "volume_threshold_multiplier=2.0\n"
        << "excessive_threshold_multiplier=3.0\n";

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
    auto_volume_threshold = reader.GetBoolean("audio", "auto_volume_threshold", auto_volume_threshold);
    auto_excessive_threshold = reader.GetBoolean("audio", "auto_excessive_threshold", auto_excessive_threshold);
    volume_threshold_multiplier = reader.GetFloat("audio", "volume_threshold_multiplier", volume_threshold_multiplier);
    excessive_threshold_multiplier = reader.GetFloat("audio", "excessive_threshold_multiplier", excessive_threshold_multiplier);

    return true;
}

bool Config::SaveToFile(const std::string& filename) const {
    std::ofstream config_file(filename);
    if (!config_file.is_open()) {
        return false;
    }

    config_file << "[connection]\n"
        << "address=" << address << "\n"
        << "port=" << port << "\n"
        << "osc_address_left=" << address_left << "\n"
        << "osc_address_right=" << address_right << "\n"
        << "osc_address_overwhelmingly_loud=" << address_overwhelmingly_loud << "\n\n"
        << "[audio]\n"
        << "differential_threshold=" << differential_threshold << "\n"
        << "volume_threshold=" << volume_threshold << "\n"
        << "excessive_volume_threshold=" << excessive_volume_threshold << "\n"
        << "reset_timeout_ms=" << reset_timeout_ms << "\n"
        << "timeout_ms=" << timeout_ms << "\n"
        << "auto_volume_threshold=" << (auto_volume_threshold ? "true" : "false") << "\n"
        << "auto_excessive_threshold=" << (auto_excessive_threshold ? "true" : "false") << "\n"
        << "volume_threshold_multiplier=" << volume_threshold_multiplier << "\n"
        << "excessive_threshold_multiplier=" << excessive_threshold_multiplier << "\n";

    return true;
}
