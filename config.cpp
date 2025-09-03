#include "config.hpp"
#include "logger.hpp"
#include <inih/INIReader.h>
#include <fstream>
#include <iostream>

// Helper function to convert LogLevel to string
std::string LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::LDEBUG: return "DEBUG";
        case LogLevel::LINFO: return "INFO";
        case LogLevel::LWARN: return "WARN";
        case LogLevel::LERROR: return "ERROR";
        default: return "WARN";
    }
}
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#else
#include <sys/stat.h>
#endif

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
    , log_level(LogLevel::LWARN)  // Default to WARN level
    , selected_device_id("")  // Empty means use default device
{
}

std::string Config::GetDefaultConfigPath() {
#ifdef _WIN32
    // Get %APPDATA% path
    CHAR appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::string configDir = std::string(appDataPath) + "\\EarPerkOSC";
        
        // Create directory if it doesn't exist
        _mkdir(configDir.c_str());
        
        return configDir + "\\config.ini";
    }
    // Fallback to current directory
    return "config.ini";
#else
    // Linux/Mac: use ~/.config/EarPerkOSC/
    const char* home = getenv("HOME");
    if (home) {
        std::string configDir = std::string(home) + "/.config/EarPerkOSC";
        mkdir(configDir.c_str(), 0755);
        return configDir + "/config.ini";
    }
    return "config.ini";
#endif
}

bool Config::CreateDefaultConfigFile(const std::string& filename) {
    std::string configPath = filename.empty() ? GetDefaultConfigPath() : filename;
    std::ofstream config_file(configPath);
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
        << "excessive_threshold_multiplier=3.0\n"
        << "selected_device_id=\n"
        << "log_level=WARN\n";

    return true;
}

bool Config::LoadFromFile(const std::string& filename) {
    std::string configPath = filename.empty() ? GetDefaultConfigPath() : filename;
    LOG_INFO_F("Loading configuration from: %s", configPath.c_str());
    
    INIReader reader(configPath);
    if (reader.ParseError() < 0) {
        // File doesn't exist or is corrupted, create a new default one
        LOG_WARN_F("Config file not found or corrupted (error: %d), creating default config at: %s", reader.ParseError(), configPath.c_str());
        std::cout << "Config file not found or corrupted, creating default config at: " << configPath << std::endl;
        
        if (!CreateDefaultConfigFile(configPath)) {
            LOG_ERROR_F("Could not create default config file at: %s", configPath.c_str());
            std::cerr << "Error: Could not create default config file at: " << configPath << std::endl;
            return false;
        }
        
        // Try to load the newly created file
        reader = INIReader(configPath);
        if (reader.ParseError() < 0) {
            LOG_ERROR_F("Could not parse newly created config file! Error: %d", reader.ParseError());
            std::cerr << "Error: Could not parse newly created config file!" << std::endl;
            return false;
        }
        LOG_INFO_F("Default config.ini created successfully at: %s", configPath.c_str());
        std::cout << "Default config.ini created successfully at: " << configPath << std::endl;
    } else {
        LOG_INFO("Configuration file loaded successfully");
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
    selected_device_id = reader.Get("audio", "selected_device_id", selected_device_id);
    
    LOG_DEBUG_F("Config loaded - selected_device_id: '%s'", selected_device_id.c_str());

    // Load log level safely
    try {
        std::string logLevelStr = reader.Get("audio", "log_level", "WARN");
        if (logLevelStr == "DEBUG") {
            log_level = LogLevel::LDEBUG;
        } else if (logLevelStr == "INFO") {
            log_level = LogLevel::LINFO;
        } else if (logLevelStr == "WARN") {
            log_level = LogLevel::LWARN;
        } else if (logLevelStr == "ERROR") {
            log_level = LogLevel::LERROR;
        } else {
            log_level = LogLevel::LWARN; // Default fallback
        }
    } catch (...) {
        // If anything goes wrong loading log level, use safe default
        log_level = LogLevel::LWARN;
    }

    return true;
}

bool Config::SaveToFile(const std::string& filename) const {
    std::string configPath = filename.empty() ? GetDefaultConfigPath() : filename;
    std::ofstream config_file(configPath);
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
        << "excessive_threshold_multiplier=" << excessive_threshold_multiplier << "\n"
        << "selected_device_id=" << selected_device_id << "\n"
        << "log_level=" << LogLevelToString(log_level) << "\n";
        
    LOG_DEBUG_F("Config saved - selected_device_id: '%s'", selected_device_id.c_str());

    return true;
}
