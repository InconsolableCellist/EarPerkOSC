#pragma once
#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <thread>
#include <atomic>
#include <deque>
#include <chrono>
#include "config.hpp"
#include "osc_sender.hpp"

class AudioProcessor {
public:
    explicit AudioProcessor(const Config& config);
    ~AudioProcessor();

    // Delete copy constructor and assignment operator
    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;

    bool Initialize();
    void Start();
    void Stop();

    // Getters for UI
    float GetLeftVolume() const { return current_left_vol; }
    float GetRightVolume() const { return current_right_vol; }
    bool IsLeftPerked() const { return left_perked; }
    bool IsRightPerked() const { return right_perked; }
    bool IsOverwhelmed() const { return overwhelmingly_loud; }

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

    // WASAPI interfaces
    IMMDeviceEnumerator* pEnumerator;
    IMMDevice* pDevice;
    IAudioClient* pAudioClient;
    IAudioCaptureClient* pCaptureClient;
    WAVEFORMATEX* pwfx;

    // Audio processing
    std::deque<uint8_t> sample_queue;
    std::atomic<bool> running;
    std::thread audioThread;

    // Configuration
    Config config;
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
};