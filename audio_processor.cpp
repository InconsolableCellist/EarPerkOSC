#include "audio_processor.hpp"
#include "logger.hpp"
#include <functional>
#include <iostream>
#include <vector>
#include <tuple>

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
    , currentDeviceId("")
    , currentDeviceName("No Device")
    , currentDeviceIsRender(true)
{
    LOG_DEBUG("AudioProcessor constructor called");
    last_left_message_timestamp = std::chrono::steady_clock::now();
    last_right_message_timestamp = std::chrono::steady_clock::now();
    last_overwhelm_timestamp = std::chrono::steady_clock::now();
    LOG_DEBUG("AudioProcessor constructor completed");
}

AudioProcessor::~AudioProcessor() {
    LOG_DEBUG("AudioProcessor destructor called");
    Stop();
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    if (pwfx) CoTaskMemFree(pwfx);
    LOG_DEBUG("AudioProcessor destructor completed");
}

bool AudioProcessor::Initialize() {
    LOG_INFO("Initializing AudioProcessor");
    
    LOG_DEBUG("Cleaning up existing audio interfaces");
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

    LOG_DEBUG("Initializing COM");
    HRESULT hr = CoInitializeEx(nullptr, COINIT_SPEED_OVER_MEMORY);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        // RPC_E_CHANGED_MODE means COM is already initialized with a different threading model
        // This is not a critical error, we can continue
        LOG_ERROR_F("Failed to initialize COM: 0x%08X", hr);
        return false;
    }
    LOG_DEBUG("COM initialized successfully");

    LOG_DEBUG("Creating MMDeviceEnumerator");
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        LOG_ERROR_F("Failed to create MMDeviceEnumerator: 0x%08X", hr);
        return false;
    }
    LOG_DEBUG("MMDeviceEnumerator created successfully");

    // Use selected device if specified, otherwise use default
    if (!config.selected_device_id.empty()) {
        LOG_DEBUG_F("Getting selected audio device: %s", config.selected_device_id.c_str());
        
        // Convert string to wide string for Windows API
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, config.selected_device_id.c_str(), -1, NULL, 0);
        std::wstring wide_device_id(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, config.selected_device_id.c_str(), -1, &wide_device_id[0], size_needed);
        wide_device_id.resize(size_needed - 1);  // Remove null terminator
        
        hr = pEnumerator->GetDevice(wide_device_id.c_str(), &pDevice);
        if (FAILED(hr)) {
            LOG_WARN_F("Failed to get selected audio device (0x%08X), falling back to default", hr);
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
            if (FAILED(hr)) {
                LOG_ERROR_F("Failed to get default audio endpoint: 0x%08X", hr);
                return false;
            }
            LOG_DEBUG("Fallback to default audio endpoint successful");
            currentDeviceIsRender = true;  // Default endpoint is always a render device
        } else {
            LOG_DEBUG("Selected audio device acquired successfully");
        }
    } else {
        LOG_DEBUG("Getting default audio endpoint");
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr)) {
            LOG_ERROR_F("Failed to get default audio endpoint: 0x%08X", hr);
            return false;
        }
        LOG_DEBUG("Default audio endpoint acquired successfully");
        currentDeviceIsRender = true;  // Default endpoint is always a render device
    }
    
    // Store current device info for UI
    LPWSTR deviceId = nullptr;
    hr = pDevice->GetId(&deviceId);
    if (SUCCEEDED(hr)) {
        int id_size_needed = WideCharToMultiByte(CP_UTF8, 0, deviceId, -1, NULL, 0, NULL, NULL);
        std::string deviceIdStr(id_size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, deviceId, -1, &deviceIdStr[0], id_size_needed, NULL, NULL);
        currentDeviceId = deviceIdStr.c_str();  // Remove null terminator
        
        // Get device name
        IPropertyStore* pPropertyStore = nullptr;
        hr = pDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
        if (SUCCEEDED(hr)) {
            PROPVARIANT friendlyName;
            PropVariantInit(&friendlyName);
            hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
            
            if (SUCCEEDED(hr) && friendlyName.vt == VT_LPWSTR) {
                int name_size_needed = WideCharToMultiByte(CP_UTF8, 0, friendlyName.pwszVal, -1, NULL, 0, NULL, NULL);
                std::string temp(name_size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, friendlyName.pwszVal, -1, &temp[0], name_size_needed, NULL, NULL);
                currentDeviceName = temp.c_str();  // Remove null terminator
            }
            
            PropVariantClear(&friendlyName);
            pPropertyStore->Release();
        }
        
        if (currentDeviceName.empty()) {
            currentDeviceName = "Unknown Device";
        }
        
        CoTaskMemFree(deviceId);
        LOG_DEBUG_F("Using audio device: %s", currentDeviceName.c_str());
    }

    LOG_DEBUG("Activating audio client");
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
        nullptr, (void**)&pAudioClient);
    if (FAILED(hr)) {
        LOG_ERROR_F("Failed to activate audio client: 0x%08X", hr);
        return false;
    }
    LOG_DEBUG("Audio client activated successfully");

    bool isRenderDevice = currentDeviceIsRender;
    LOG_DEBUG_F("Using stored device type: isRenderDevice=%s", isRenderDevice ? "true" : "false");

    LOG_DEBUG("Getting audio mix format");
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        LOG_ERROR_F("Failed to get mix format: 0x%08X", hr);
        return false;
    }
    LOG_DEBUG_F("Audio format: %d channels, %d Hz, %d bits", pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample);

    DWORD streamFlags = 0;
    
    bool isVoiceMeeterDevice = (currentDeviceName.find("VoiceMeeter") != std::string::npos ||
                                currentDeviceName.find("VAIO") != std::string::npos ||
                                currentDeviceName.find("VB-Audio") != std::string::npos);
    
    if (isVoiceMeeterDevice) {
        streamFlags = 0;
        LOG_DEBUG("VoiceMeeter device detected - using direct capture (no loopback)");
    } else {
        streamFlags = isRenderDevice ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;
    }
    
    LOG_DEBUG_F("Initializing audio client with flags: 0x%08X (isRenderDevice=%s, isVoiceMeeter=%s)", 
                streamFlags, isRenderDevice ? "true" : "false", isVoiceMeeterDevice ? "true" : "false");
    
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        streamFlags,
        BUFFER_DURATION,
        0,
        pwfx,
        nullptr);
        
    // Handle device in use error by trying with different buffer settings
    if (hr == AUDCLNT_E_DEVICE_IN_USE) {
        LOG_DEBUG("Device in use, trying with auto buffer duration");
        hr = pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            streamFlags,
            0,  // Let Windows choose buffer duration
            0,
            pwfx,
            nullptr);
            
        if (SUCCEEDED(hr)) {
            LOG_DEBUG("Successfully initialized with auto buffer duration");
        }
    }
        
    // If the mix format is not supported, try fallback formats
    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
        LOG_DEBUG("Mix format not supported for loopback, trying fallback formats");
        
        // Free the original format
        CoTaskMemFree(pwfx);
        pwfx = nullptr;
        
        // Try common fallback formats that are usually supported
        std::vector<std::tuple<DWORD, DWORD, WORD>> fallbackFormats = {
            {44100, 2, 16},  // 44.1kHz, 2 channels, 16-bit
            {48000, 2, 16},  // 48kHz, 2 channels, 16-bit  
            {44100, 2, 24},  // 44.1kHz, 2 channels, 24-bit
            {48000, 2, 24},  // 48kHz, 2 channels, 24-bit
            {44100, 2, 32},  // 44.1kHz, 2 channels, 32-bit
            {48000, 2, 32}   // 48kHz, 2 channels, 32-bit
        };
        
        bool formatFound = false;
        for (const auto& [sampleRate, channels, bitsPerSample] : fallbackFormats) {
            // Create a new format structure
            pwfx = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
            if (!pwfx) {
                LOG_ERROR("Failed to allocate memory for audio format");
                return false;
            }
            
            pwfx->wFormatTag = WAVE_FORMAT_PCM;
            pwfx->nChannels = channels;
            pwfx->nSamplesPerSec = sampleRate;
            pwfx->wBitsPerSample = bitsPerSample;
            pwfx->nBlockAlign = (channels * bitsPerSample) / 8;
            pwfx->nAvgBytesPerSec = sampleRate * pwfx->nBlockAlign;
            pwfx->cbSize = 0;
            
            LOG_DEBUG_F("Trying fallback format: %d channels, %d Hz, %d bits", 
                       pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample);
            
            // Check if this format is supported
            WAVEFORMATEX* pClosestMatch = nullptr;
            hr = pAudioClient->IsFormatSupported(
                AUDCLNT_SHAREMODE_SHARED,
                pwfx,
                &pClosestMatch);
                
            if (hr == S_OK) {
                // Format is supported exactly, try to initialize
                hr = pAudioClient->Initialize(
                    AUDCLNT_SHAREMODE_SHARED,
                    streamFlags,
                    BUFFER_DURATION,
                    0,
                    pwfx,
                    nullptr);
                    
                // If device is in use, try with auto buffer duration
                if (hr == AUDCLNT_E_DEVICE_IN_USE) {
                    LOG_DEBUG("Device in use with fallback format, trying auto buffer duration");
                    hr = pAudioClient->Initialize(
                        AUDCLNT_SHAREMODE_SHARED,
                        streamFlags,
                        0,  // Let Windows choose buffer duration
                        0,
                        pwfx,
                        nullptr);
                }
                    
                if (SUCCEEDED(hr)) {
                    LOG_DEBUG_F("Successfully initialized with fallback format: %d channels, %d Hz, %d bits", 
                               pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample);
                    formatFound = true;
                    break;
                }
            } else if (hr == S_FALSE && pClosestMatch) {
                // Format is not supported exactly, but a close match was suggested
                LOG_DEBUG_F("Trying closest match format: %d channels, %d Hz, %d bits", 
                           pClosestMatch->nChannels, pClosestMatch->nSamplesPerSec, pClosestMatch->wBitsPerSample);
                
                // Free our format and use the suggested one
                CoTaskMemFree(pwfx);
                pwfx = pClosestMatch;
                
                hr = pAudioClient->Initialize(
                    AUDCLNT_SHAREMODE_SHARED,
                    streamFlags,
                    BUFFER_DURATION,
                    0,
                    pwfx,
                    nullptr);
                    
                // If device is in use, try with auto buffer duration
                if (hr == AUDCLNT_E_DEVICE_IN_USE) {
                    LOG_DEBUG("Device in use with closest match format, trying auto buffer duration");
                    hr = pAudioClient->Initialize(
                        AUDCLNT_SHAREMODE_SHARED,
                        streamFlags,
                        0,  // Let Windows choose buffer duration
                        0,
                        pwfx,
                        nullptr);
                }
                    
                if (SUCCEEDED(hr)) {
                    LOG_DEBUG_F("Successfully initialized with closest match format: %d channels, %d Hz, %d bits", 
                               pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample);
                    formatFound = true;
                    break;
                }
            } else {
                // Clean up the suggested format if any
                if (pClosestMatch) {
                    CoTaskMemFree(pClosestMatch);
                }
            }
            
            // This format didn't work, free it and try the next one
            CoTaskMemFree(pwfx);
            pwfx = nullptr;
        }
        
        if (!formatFound) {
            LOG_ERROR("No supported audio format found for loopback capture");
            return false;
        }
    } else if (FAILED(hr)) {
        if (hr == AUDCLNT_E_DEVICE_IN_USE) {
            LOG_ERROR("Audio device is in use by another application. Please check:");
            LOG_ERROR("1. Close other audio applications that might be using exclusive mode");
            LOG_ERROR("2. Disable exclusive mode in Sound settings > Device Properties > Advanced");
            LOG_ERROR("3. Disable audio enhancement software (e.g., Nahimic, Sonic Studio)");
        } else {
            LOG_ERROR_F("Failed to initialize audio client: 0x%08X", hr);
        }
        return false;
    }
    LOG_DEBUG("Audio client initialized successfully");

    LOG_DEBUG("Getting audio capture client service");
    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&pCaptureClient);
    if (FAILED(hr)) {
        LOG_ERROR_F("Failed to get audio capture client: 0x%08X", hr);
        return false;
    }
    LOG_DEBUG("Audio capture client acquired successfully");

    LOG_INFO("AudioProcessor initialization completed successfully");
    return true;
}

void AudioProcessor::Start() {
    if (!running) {
        LOG_INFO("Starting audio processor");
        running = true;
        
        HRESULT hr = pAudioClient->Start();
        if (FAILED(hr)) {
            LOG_ERROR_F("Failed to start audio client: 0x%08X", hr);
            running = false;
            return;
        }
        LOG_DEBUG("Audio client started successfully");
        
        LOG_DEBUG("Starting audio processing thread");
        audioThread = std::thread(&AudioProcessor::ProcessAudio, this);
        LOG_INFO("Audio processor started successfully");
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
        LOG_DEBUG("Current audio device is no longer active");
        return false;
    }
    
    // If we have a specific selected device, only check if it's still active
    // Don't check if it's still the default - we want to stick with the selected device
    if (!config.selected_device_id.empty()) {
        LOG_DEBUG("Using selected device - skipping default device check");
        return true;
    }
    
    // Only check for default device changes if we're using the default device
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
            LOG_DEBUG("Default audio device changed, marking for reconnection");
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

std::vector<AudioProcessor::AudioDevice> AudioProcessor::GetAvailableDevices() {
    std::vector<AudioDevice> devices;
    
    IMMDeviceEnumerator* pTempEnumerator = nullptr;
    IMMDeviceCollection* pCollection = nullptr;
    
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pTempEnumerator);
    
    if (FAILED(hr)) {
        LOG_ERROR_F("Failed to create device enumerator for listing: 0x%08X", hr);
        return devices;
    }
    
    // Get default device to mark it
    IMMDevice* pDefaultDevice = nullptr;
    LPWSTR defaultDeviceId = nullptr;
    hr = pTempEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDefaultDevice);
    if (SUCCEEDED(hr)) {
        pDefaultDevice->GetId(&defaultDeviceId);
    }
    
    // Enumerate both render and capture devices for comprehensive device list
    EDataFlow dataFlows[] = { eRender, eCapture };
    const char* flowNames[] = { "Render", "Capture" };
    
    for (int flowIndex = 0; flowIndex < 2; flowIndex++) {
        EDataFlow dataFlow = dataFlows[flowIndex];
        
        hr = pTempEnumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &pCollection);
        if (SUCCEEDED(hr)) {
            UINT deviceCount = 0;
            hr = pCollection->GetCount(&deviceCount);
            
            if (SUCCEEDED(hr)) {
                for (UINT i = 0; i < deviceCount; i++) {
                IMMDevice* pDevice = nullptr;
                hr = pCollection->Item(i, &pDevice);
                
                if (SUCCEEDED(hr)) {
                    LPWSTR deviceId = nullptr;
                    hr = pDevice->GetId(&deviceId);
                    
                    if (SUCCEEDED(hr)) {
                        // Get device friendly name
                        IPropertyStore* pPropertyStore = nullptr;
                        hr = pDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
                        
                        std::string deviceName = "Unknown Device";
                        if (SUCCEEDED(hr)) {
                            PROPVARIANT friendlyName;
                            PropVariantInit(&friendlyName);
                            hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
                            
                            if (SUCCEEDED(hr) && friendlyName.vt == VT_LPWSTR) {
                                // Convert wide string to regular string
                                int size_needed = WideCharToMultiByte(CP_UTF8, 0, friendlyName.pwszVal, -1, NULL, 0, NULL, NULL);
                                std::string temp(size_needed, 0);
                                WideCharToMultiByte(CP_UTF8, 0, friendlyName.pwszVal, -1, &temp[0], size_needed, NULL, NULL);
                                deviceName = temp.c_str();  // Remove null terminator
                            }
                            
                            PropVariantClear(&friendlyName);
                            pPropertyStore->Release();
                        }
                        
                        // Convert device ID to string
                        int id_size_needed = WideCharToMultiByte(CP_UTF8, 0, deviceId, -1, NULL, 0, NULL, NULL);
                        std::string deviceIdStr(id_size_needed, 0);
                        WideCharToMultiByte(CP_UTF8, 0, deviceId, -1, &deviceIdStr[0], id_size_needed, NULL, NULL);
                        deviceIdStr = deviceIdStr.c_str();  // Remove null terminator
                        
                        bool isDefault = false;
                        if (defaultDeviceId) {
                            isDefault = (wcscmp(deviceId, defaultDeviceId) == 0);
                        }
                        
                        // Add helpful identification for VoiceMeeter devices and device type
                        std::string deviceTypeLabel = (dataFlow == eRender) ? " (Output)" : " (Input)";
                        
                        if (deviceName.find("VoiceMeeter") != std::string::npos || 
                            deviceName.find("VAIO") != std::string::npos ||
                            deviceName.find("VB-Audio") != std::string::npos) {
                            deviceName += " [VoiceMeeter Virtual Device]";
                        }
                        
                        deviceName += deviceTypeLabel;
                        
                        bool isRenderDevice = (dataFlow == eRender);
                        devices.push_back({deviceIdStr, deviceName, isDefault, isRenderDevice});
                        
                        CoTaskMemFree(deviceId);
                    }
                    
                    pDevice->Release();
                }
            }
        }
        
        pCollection->Release();
        pCollection = nullptr;
    }
    } // End dataFlow loop
    
    if (defaultDeviceId) CoTaskMemFree(defaultDeviceId);
    if (pDefaultDevice) pDefaultDevice->Release();
    pTempEnumerator->Release();
    
    return devices;
}

bool AudioProcessor::SetSelectedDevice(const std::string& deviceId) {
    LOG_DEBUG_F("Setting selected device ID to: '%s'", deviceId.c_str());
    
    // Find the device in our available devices list to get its type
    auto devices = GetAvailableDevices();
    for (const auto& device : devices) {
        if (device.id == deviceId) {
            currentDeviceIsRender = device.isRenderDevice;
            LOG_DEBUG_F("Device type: isRenderDevice=%s", currentDeviceIsRender ? "true" : "false");
            break;
        }
    }
    
    config.selected_device_id = deviceId;
    
    // Restart audio with new device
    return RestartAudio();
}

std::string AudioProcessor::GetCurrentDeviceId() const {
    return currentDeviceId;
}

std::string AudioProcessor::GetCurrentDeviceName() const {
    return currentDeviceName;
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
