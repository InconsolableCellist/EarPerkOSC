#pragma once
#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <thread>
#include <atomic>
#include <deque>
#include <chrono>
#include <vector>
#include <string>
#include "config.hpp"
#include "osc_sender.hpp"
#include "volume_analyzer.hpp"

class AudioProcessor {
public:
    explicit AudioProcessor(Config& config);
    ~AudioProcessor();

    // Delete copy constructor and assignment operator
    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;

    bool Initialize();
    void Start();
    void Stop();
    
    // Force a complete audio system restart (for UI button)
    bool RestartAudio();

    // Device enumeration for UI
    struct AudioDevice {
        std::string id;
        std::string name;
        bool isDefault;
        bool isRenderDevice;  // true for render/output devices, false for capture/input devices
    };
    std::vector<AudioDevice> GetAvailableDevices();
    bool SetSelectedDevice(const std::string& deviceId);
    std::string GetCurrentDeviceId() const;
    std::string GetCurrentDeviceName() const;

    // Getters for UI
    float GetLeftVolume() const { return current_left_vol; }
    float GetRightVolume() const { return current_right_vol; }
    bool IsLeftPerked() const { return left_perked; }
    bool IsRightPerked() const { return right_perked; }
    bool IsOverwhelmed() const { return overwhelmingly_loud; }
    bool IsAudioWorking() const { return pAudioClient != nullptr && pCaptureClient != nullptr; }

    void UpdateThresholds(float differential, float volume, float excessive) {
        config.differential_threshold = differential;
        config.volume_threshold = volume;
        config.excessive_volume_threshold = excessive;
    }

private:
    void ProcessAudio();
    std::pair<float, float> CalculateAvgLR();
    void ProcessVolPerkAndReset(float left_avg, float right_avg);
    void ProcessVolOverwhelm(float left_avg, float right_avg);
    bool CheckDeviceStatus();
    bool TryReconnectDevice();

    // WASAPI interfaces
    IMMDeviceEnumerator* pEnumerator;
    IMMDevice* pDevice;
    IAudioClient* pAudioClient;
    IAudioCaptureClient* pCaptureClient;
    WAVEFORMATEX* pwfx;

    // Audio processing
    std::deque<uint8_t> sample_queue;
    std::atomic<bool> running;
    std::atomic<bool> needsReconnect;
    std::thread audioThread;
    VolumeAnalyzer volume_analyzer;

    // Configuration
    Config& config;
    OSCSender osc;

    // State variables
    bool left_perked;
    bool right_perked;
    bool overwhelmingly_loud;
    std::chrono::steady_clock::time_point last_left_message_timestamp;
    std::chrono::steady_clock::time_point last_right_message_timestamp;
    std::chrono::steady_clock::time_point last_overwhelm_timestamp;
    float current_left_vol;
    float current_right_vol;
    
    // Device selection
    std::string currentDeviceId;
    std::string currentDeviceName;
    bool currentDeviceIsRender;
};