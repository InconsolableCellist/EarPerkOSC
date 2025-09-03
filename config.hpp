#pragma once
#include <string>
#include "logger.hpp"

struct Config {
    std::string address;
    int port;
    std::string address_left;
    std::string address_right;
    std::string address_overwhelmingly_loud;
    bool auto_volume_threshold;
    bool auto_excessive_threshold;
    float volume_threshold_multiplier;
    float excessive_threshold_multiplier;
    LogLevel log_level;

    float differential_threshold;
    float volume_threshold;
    float excessive_volume_threshold;
    int reset_timeout_ms;
    int timeout_ms;
    
    // Audio device selection
    std::string selected_device_id;

    // Default constructor with reasonable defaults
    Config();

    // Load configuration from file, returns true if successful
    bool LoadFromFile(const std::string& filename = "");

    // Create default config file if it doesn't exist
    static bool CreateDefaultConfigFile(const std::string& filename = "");
    bool SaveToFile(const std::string& filename = "") const;

    // Get the default config file path in AppData
    static std::string GetDefaultConfigPath();
};