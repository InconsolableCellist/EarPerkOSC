#include "audio_processor.hpp"
#include <functional>
#include <iostream>

AudioProcessor::AudioProcessor(Config& config)
    : pEnumerator(nullptr)
    , pDevice(nullptr)
    , pAudioClient(nullptr)
    , pCaptureClient(nullptr)
    , pwfx(nullptr)
    , running(false)
    , needsReconnect(false)
    , config(config)
    , osc(config)
    , left_perked(false)
    , right_perked(false)
    , overwhelmingly_loud(false)
    , current_left_vol(0.0f)
    , current_right_vol(0.0f)
{
    last_left_message_timestamp = std::chrono::steady_clock::now();
    last_right_message_timestamp = std::chrono::steady_clock::now();
    last_overwhelm_timestamp = std::chrono::steady_clock::now();
}

AudioProcessor::~AudioProcessor() {
    Stop();
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    if (pwfx) CoTaskMemFree(pwfx);
}

bool AudioProcessor::Initialize() {
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    if (pwfx) CoTaskMemFree(pwfx);
    
    // Reset pointers
    pCaptureClient = nullptr;
    pAudioClient = nullptr;
    pDevice = nullptr;
    pEnumerator = nullptr;
    pwfx = nullptr;

    const UINT32 REFTIMES_PER_SEC = 10000000;
    const REFERENCE_TIME BUFFER_DURATION = REFTIMES_PER_SEC / 100; // 10ms buffer

    HRESULT hr = CoInitializeEx(nullptr, COINIT_SPEED_OVER_MEMORY);
    if (FAILED(hr)) return false;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return false;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) return false;

    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
        nullptr, (void**)&pAudioClient);
    if (FAILED(hr)) return false;

    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) return false;

    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        BUFFER_DURATION,
        0,
        pwfx,
        nullptr);
    if (FAILED(hr)) return false;

    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&pCaptureClient);
    if (FAILED(hr)) return false;

    return true;
}

void AudioProcessor::Start() {
    if (!running) {
        running = true;
        pAudioClient->Start();
        audioThread = std::thread(&AudioProcessor::ProcessAudio, this);
    }
}

void AudioProcessor::Stop() {
    if (running) {
        running = false;
        if (audioThread.joinable()) {
            audioThread.join();
        }
        if (pAudioClient) {
            pAudioClient->Stop();
        }
    }
}

bool AudioProcessor::RestartAudio() {
    std::cout << "Manual audio restart requested..." << std::endl;
    
    // Stop current processing
    Stop();
    
    // Clear the reconnect flag in case it was set
    needsReconnect.store(false);
    
    // Force complete reinitialization (this will get the current default device)
    if (!Initialize()) {
        std::cerr << "Failed to reinitialize audio system!" << std::endl;
        return false;
    }
    
    // Restart processing
    Start();
    
    std::cout << "Audio system restarted successfully with current default device." << std::endl;
    return true;
}

bool AudioProcessor::CheckDeviceStatus() {
    if (!pDevice || !pEnumerator) return false;
    
    // Check if current device is still active
    DWORD state;
    HRESULT hr = pDevice->GetState(&state);
    if (FAILED(hr) || state != DEVICE_STATE_ACTIVE) {
        return false;
    }
    
    // Check if current device is still the default
    IMMDevice* pCurrentDefault = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pCurrentDefault);
    if (SUCCEEDED(hr) && pCurrentDefault) {
        LPWSTR currentDeviceId = nullptr;
        LPWSTR defaultDeviceId = nullptr;
        
        hr = pDevice->GetId(&currentDeviceId);
        HRESULT hr2 = pCurrentDefault->GetId(&defaultDeviceId);
        
        bool isStillDefault = true;
        if (SUCCEEDED(hr) && SUCCEEDED(hr2)) {
            isStillDefault = (wcscmp(currentDeviceId, defaultDeviceId) == 0);
        }
        
        if (currentDeviceId) CoTaskMemFree(currentDeviceId);
        if (defaultDeviceId) CoTaskMemFree(defaultDeviceId);
        pCurrentDefault->Release();
        
        if (!isStillDefault) {
            std::cout << "Default audio device has changed, marking for reconnection..." << std::endl;
            return false;
        }
    }
    
    return true;
}

bool AudioProcessor::TryReconnectDevice() {
    // Release current interfaces
    if (pCaptureClient) {
        pCaptureClient->Release();
        pCaptureClient = nullptr;
    }
    if (pAudioClient) {
        pAudioClient->Stop();
        pAudioClient->Release();
        pAudioClient = nullptr;
    }
    if (pDevice) {
        pDevice->Release();
        pDevice = nullptr;
    }
    if (pEnumerator) {
        pEnumerator->Release();
        pEnumerator = nullptr;
    }
    if (pwfx) {
        CoTaskMemFree(pwfx);
        pwfx = nullptr;
    }

    // Try to reinitialize
    if (!Initialize()) {
        return false;
    }

    // Restart audio capture
    HRESULT hr = pAudioClient->Start();
    return SUCCEEDED(hr);
}

void AudioProcessor::ProcessAudio() {
    while (running) {
        // Check if we need to reconnect
        if (needsReconnect.load()) {
            std::cout << "Audio device reconnection needed..." << std::endl;
            if (TryReconnectDevice()) {
                std::cout << "Audio device reconnected successfully." << std::endl;
                needsReconnect.store(false);
            } else {
                std::cout << "Audio device reconnection failed, retrying in 1 second..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }

        // Check device status and mark for reconnection if needed
        if (!CheckDeviceStatus()) {
            std::cout << "Audio device disconnected, marking for reconnection..." << std::endl;
            needsReconnect.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Sleep(1);
        UINT32 packetLength = 0;
        HRESULT hr = pCaptureClient->GetNextPacketSize(&packetLength);
        
        // Check for device disconnection errors
        if (FAILED(hr)) {
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
                std::cout << "Audio device invalidated, marking for reconnection..." << std::endl;
                needsReconnect.store(true);
                continue;
            } else {
                std::cout << "Unexpected error in GetNextPacketSize: " << std::hex << hr << std::endl;
                break;
            }
        }

        while (packetLength > 0) {
            BYTE* data;
            UINT32 numFramesAvailable;
            DWORD flags;

            hr = pCaptureClient->GetBuffer(
                &data,
                &numFramesAvailable,
                &flags,
                nullptr,
                nullptr);
            
            if (FAILED(hr)) {
                if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
                    std::cout << "Audio device invalidated during GetBuffer, marking for reconnection..." << std::endl;
                    needsReconnect.store(true);
                    break;
                } else {
                    std::cout << "Unexpected error in GetBuffer: " << std::hex << hr << std::endl;
                    break;
                }
            }

            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                size_t bytesPerFrame = pwfx->nBlockAlign;
                for (UINT32 i = 0; i < numFramesAvailable * bytesPerFrame; i++) {
                    sample_queue.push_back(data[i]);
                }
            }

            hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
            if (FAILED(hr)) {
                if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
                    std::cout << "Audio device invalidated during ReleaseBuffer, marking for reconnection..." << std::endl;
                    needsReconnect.store(true);
                    break;
                } else {
                    std::cout << "Unexpected error in ReleaseBuffer: " << std::hex << hr << std::endl;
                    break;
                }
            }

            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) {
                if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
                    std::cout << "Audio device invalidated during packet size check, marking for reconnection..." << std::endl;
                    needsReconnect.store(true);
                    break;
                } else {
                    std::cout << "Unexpected error in GetNextPacketSize loop: " << std::hex << hr << std::endl;
                    break;
                }
            }
        }
        
        // If we marked for reconnection, continue to the next iteration
        if (needsReconnect.load()) {
            continue;
        }

        auto [left_avg, right_avg] = CalculateAvgLR();

        // std::cout << "UI auto_volume_threshold: " << config.auto_volume_threshold << std::endl;
        // std::cout << "ProcessAudio auto_volume_threshold: " << config.auto_volume_threshold << std::endl;

        // Only process volume analysis at reduced rate
        static int processCounter = 0;
        if (++processCounter % 10 == 0) {  // Process every 10th iteration
            if (volume_analyzer.ShouldUpdate()) {
                volume_analyzer.AddSample(left_avg, right_avg);
                volume_analyzer.UpdateTimestamp();

                if (config.auto_volume_threshold || config.auto_excessive_threshold) {
                    auto [vol_threshold, excess_threshold] = 
                        volume_analyzer.GetSuggestedThresholds(
                            config.volume_threshold_multiplier,
                            config.excessive_threshold_multiplier);

                    if (config.auto_volume_threshold) {
                        config.volume_threshold = vol_threshold;
                    }
                    if (config.auto_excessive_threshold) {
                        config.excessive_volume_threshold = excess_threshold;
                    }
                }
            }
        }

        ProcessVolOverwhelm(left_avg, right_avg);
        if (!overwhelmingly_loud) {
            ProcessVolPerkAndReset(left_avg, right_avg);
        }
    }
}

std::pair<float, float> AudioProcessor::CalculateAvgLR() {
    float left_volume = 0.0f;
    float right_volume = 0.0f;
    size_t count = 0;

    while (sample_queue.size() >= pwfx->nChannels * sizeof(float)) {
        std::array<uint8_t, sizeof(float)> left_bytes;
        std::array<uint8_t, sizeof(float)> right_bytes;

        for (size_t i = 0; i < sizeof(float); i++) {
            left_bytes[i] = sample_queue.front();
            sample_queue.pop_front();
        }
        for (size_t i = 0; i < sizeof(float); i++) {
            right_bytes[i] = sample_queue.front();
            sample_queue.pop_front();
        }

        float left = *reinterpret_cast<float*>(left_bytes.data());
        float right = *reinterpret_cast<float*>(right_bytes.data());

        left_volume += std::abs(left);
        right_volume += std::abs(right);
        count++;
    }

    if (count > 0) {
        current_left_vol = left_volume / count;
        current_right_vol = right_volume / count;
    }

    return { current_left_vol, current_right_vol };
}

void AudioProcessor::ProcessVolPerkAndReset(float left_avg, float right_avg) {
    auto current_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(config.timeout_ms);
    auto reset_timeout = std::chrono::milliseconds(config.reset_timeout_ms);

    if (left_avg > config.differential_threshold
        && right_avg > config.differential_threshold
        && left_avg > config.volume_threshold
        && right_avg > config.volume_threshold) {
        if (current_time - last_left_message_timestamp > timeout &&
            current_time - last_right_message_timestamp > timeout) {
            osc.SendLeftEar(true);
            osc.SendRightEar(true);
            last_left_message_timestamp = current_time;
            last_right_message_timestamp = current_time;
            left_perked = right_perked = true;
        }
    }
    else if ((left_avg - right_avg > config.differential_threshold) && left_avg > config.volume_threshold) {
        if (current_time - last_left_message_timestamp > timeout) {
            osc.SendLeftEar(true);
            last_left_message_timestamp = current_time;
            left_perked = true;
        }
    }
    else if ((right_avg - left_avg > config.differential_threshold) && right_avg > config.volume_threshold) {
        if (current_time - last_right_message_timestamp > timeout) {
            osc.SendRightEar(true);
            last_right_message_timestamp = current_time;
            right_perked = true;
        }
    }

    // Reset logic
    if (left_perked && current_time - last_left_message_timestamp > reset_timeout) {
        osc.SendLeftEar(false);
        left_perked = false;
    }
    if (right_perked && current_time - last_right_message_timestamp > reset_timeout) {
        osc.SendRightEar(false);
        right_perked = false;
    }
}

void AudioProcessor::ProcessVolOverwhelm(float left_avg, float right_avg) {
    auto current_time = std::chrono::steady_clock::now();
    auto reset_timeout = std::chrono::milliseconds(config.reset_timeout_ms);

    if (left_avg > config.excessive_volume_threshold || right_avg > config.excessive_volume_threshold) {
        osc.SendOverwhelm(true);
        last_overwhelm_timestamp = current_time;
        overwhelmingly_loud = true;
    }
    else if (overwhelmingly_loud && current_time - last_overwhelm_timestamp > reset_timeout) {
        osc.SendOverwhelm(false);
        overwhelmingly_loud = false;
    }
}
