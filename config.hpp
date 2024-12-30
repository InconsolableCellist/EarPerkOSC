#pragma once
#include <string>

struct Config {
    std::string address;
    int port;
    std::string address_left;
    std::string address_right;
    std::string address_overwhelmingly_loud;

    float differential_threshold;
    float volume_threshold;
    float excessive_volume_threshold;
    int reset_timeout_ms;
    int timeout_ms;

    // Default constructor with reasonable defaults
    Config();

    // Load configuration from file, returns true if successful
    bool LoadFromFile(const std::string& filename = "config.ini");

    // Create default config file if it doesn't exist
    static bool CreateDefaultConfigFile(const std::string& filename = "config.ini");
    bool SaveToFile(const std::string& filename = "config.ini") const;
};