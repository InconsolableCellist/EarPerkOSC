#include "app.hpp"
#include "logger.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm>
#include <glad/glad.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

EarPerkApp::EarPerkApp() 
    : window(nullptr)
    , lastDeviceRefresh(std::chrono::steady_clock::now() - std::chrono::seconds(10)) { // Force initial refresh
	try {
		LOG_DEBUG("EarPerkApp constructor called");
	} catch (...) {
		// If logging fails during construction, continue silently
	}
	
	try {
		ImGui::SetCurrentContext(nullptr);  // Ensure no context exists before creation
		LOG_DEBUG("ImGui context cleared");
	} catch (...) {
		// If ImGui or logging fails, continue silently
	}
}

EarPerkApp::~EarPerkApp() {
    try {
        LOG_DEBUG("EarPerkApp destructor called");
        
        // Save configuration before shutting down
        LOG_DEBUG("Saving configuration before shutdown");
        try {
            SaveConfiguration();
            LOG_DEBUG("Configuration saved successfully");
        } catch (const std::exception& e) {
            LOG_ERROR_F("Exception saving configuration: %s", e.what());
        } catch (...) {
            LOG_ERROR("Unknown exception saving configuration");
        }
        
        LOG_DEBUG("Shutting down ImGui");
        // Shutdown ImGui backends in correct order
        if (ImGui::GetCurrentContext()) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        if (window) {
            LOG_DEBUG("Destroying GLFW window");
            glfwDestroyWindow(window);
        }
        
        LOG_DEBUG("Terminating GLFW");
        glfwTerminate();
        
        LOG_DEBUG("EarPerkApp destructor completed");
    }
    catch (...) {
        // Ensure destructor doesn't throw
        try {
            // Cleanup ImGui first if it exists
            if (ImGui::GetCurrentContext()) {
                ImGui_ImplOpenGL3_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
            }
            if (window) {
                glfwDestroyWindow(window);
            }
            glfwTerminate();
        }
        catch (...) {
            // Final cleanup attempt, ignore all errors
        }
    }
}

bool EarPerkApp::Initialize() {
    LOG_INFO("Starting EarPerkApp initialization");
    
    // Load configuration (create default if it doesn't exist)
    LOG_INFO("Loading configuration file");
    bool configLoaded = false;
    try {
        configLoaded = config.LoadFromFile();
        if (!configLoaded) {
            LOG_WARN("Could not load config.ini, using defaults");
            std::cerr << "Warning: Could not load config.ini, using defaults" << std::endl;
            // Continue with default values - don't fail initialization
        } else {
            LOG_INFO("Configuration loaded successfully");
            LOG_DEBUG_F("Loaded selected device ID: '%s'", config.selected_device_id.c_str());
        }
    } catch (const std::exception& e) {
        LOG_ERROR_F("Exception loading configuration: %s", e.what());
        std::cerr << "Exception loading configuration: " << e.what() << std::endl;
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception loading configuration");
        std::cerr << "Unknown exception loading configuration" << std::endl;
        return false;
    }
    
    // Apply log level from configuration immediately after loading
    Logger::getInstance().SetLevel(config.log_level);

    // Initialize GLFW
    LOG_INFO("Initializing GLFW");
    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW");
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    LOG_INFO("GLFW initialized successfully");

    // Create window with graphics context
    LOG_INFO("Setting up OpenGL context");
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    LOG_INFO_F("Creating GLFW window (%dx%d): %s", WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (!window) {
        LOG_ERROR("Failed to create GLFW window");
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }
    LOG_INFO("GLFW window created successfully");

    // Set window icon
    LOG_INFO("Setting window icon");
    SetWindowIcon();

    LOG_INFO("Making OpenGL context current");
    glfwMakeContextCurrent(window);
    
    LOG_INFO("Initializing GLAD");
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_ERROR("Failed to initialize GLAD");
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    LOG_INFO("GLAD initialized successfully");
    
    LOG_INFO("Enabling VSync");
    glfwSwapInterval(1); // Enable vsync

    // Initialize Dear ImGui
    LOG_INFO("Initializing Dear ImGui");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    LOG_INFO("Setting up ImGui backends");
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    LOG_INFO("Setting up ImGui style");
    SetupImGuiStyle();
    LOG_INFO("Dear ImGui initialized successfully");

    // Initialize audio processor
    LOG_INFO("Creating audio processor");
    audioProcessor = std::make_unique<AudioProcessor>(std::ref(config));
    
    LOG_INFO("Attempting to initialize audio processor");
    if (!audioProcessor->Initialize()) {
        LOG_WARN("Failed to initialize audio processor on startup - UI will still load");
        std::cout << "Warning: Failed to initialize audio processor - you can still use the UI to select a different device" << std::endl;
        // Don't return false - continue with UI initialization
    } else {
        LOG_INFO("Audio processor initialized successfully");
        LOG_INFO("Starting audio processor");
        audioProcessor->Start();
    }

    LOG_INFO("Setting up GLFW window callbacks");
    glfwSetWindowUserPointer(window, this);
    glfwSetWindowFocusCallback(window, WindowFocusCallback);
    glfwSetWindowIconifyCallback(window, WindowIconifyCallback);
    
    LOG_INFO("EarPerkApp initialization completed successfully");
    return true;
}

void EarPerkApp::Run() {
    LOG_INFO("Starting main application loop");
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Skip rendering when minimized
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) ||
            !glfwGetWindowAttrib(window, GLFW_VISIBLE)) {
            continue;
        }

        // Clear the background
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        RenderUI();

        glfwSwapBuffers(window);
    }
    
    LOG_INFO("Main application loop ended");
}

void EarPerkApp::RenderUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Set up main window with proper flags
    ImGuiWindowFlags window_flags = static_cast<ImGuiWindowFlags>(
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    // Get the GLFW window size
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Set the window position to (0,0) and size to full window
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width), static_cast<float>(height)));

    ImGui::Begin("EarPerk OSC", nullptr, window_flags);

    DrawVolumeMeters();
    ImGui::Separator();
    DrawStatusIndicators();
    DrawAudioDeviceSelection();
    DrawConfigurationPanel();
    DrawStatusText();

    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 25);
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "EarPerkOSC v1.3 by Foxipso - foxipso.com");

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EarPerkApp::DrawVolumeMeters() {
    ImGui::Text("Volume Levels");
    
    float left_vol = audioProcessor->GetLeftVolume();
    float right_vol = audioProcessor->GetRightVolume();
    
    // Use a scale that accommodates the max threshold range (up to 1.0)
    // This ensures threshold indicators stay within the progress bar range
    const float display_scale = 1.0f;  // Display volume in 0.0-1.0 range
    left_vol = std::min(left_vol * display_scale, 1.0f);
    right_vol = std::min(right_vol * display_scale, 1.0f);

    // Calculate threshold positions (these will be in 0.0-1.0 range since sliders max at 1.0)
    float thresh_pos = config.volume_threshold;  // Already in 0.0-0.5 range
    float excess_pos = config.excessive_volume_threshold;  // Already in 0.05-1.0 range
    
    ImGui::Text("Left Channel"); 
    ImVec2 pos = ImGui::GetCursorPos();
    ImGui::ProgressBar(left_vol, ImVec2(-1.0f, 20.0f));
    
    // Draw threshold markers
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 bar_start = ImGui::GetItemRectMin();
    ImVec2 bar_end = ImGui::GetItemRectMax();
    float bar_width = bar_end.x - bar_start.x;
    
    // Volume threshold line
    float x_pos = bar_start.x + bar_width * thresh_pos;
    draw_list->AddLine(
        ImVec2(x_pos, bar_start.y), 
        ImVec2(x_pos, bar_end.y),
        IM_COL32(255, 255, 0, 255), 2.0f);
        
    // Excessive volume threshold line
    x_pos = bar_start.x + bar_width * excess_pos;
    draw_list->AddLine(
        ImVec2(x_pos, bar_start.y), 
        ImVec2(x_pos, bar_end.y),
        IM_COL32(255, 0, 0, 255), 2.0f);

    // Repeat for right channel
    ImGui::Text("Right Channel");
    ImGui::ProgressBar(right_vol, ImVec2(-1.0f, 20.0f));
    
    bar_start = ImGui::GetItemRectMin();
    bar_end = ImGui::GetItemRectMax();
    bar_width = bar_end.x - bar_start.x;
    
    x_pos = bar_start.x + bar_width * thresh_pos;
    draw_list->AddLine(
        ImVec2(x_pos, bar_start.y), 
        ImVec2(x_pos, bar_end.y),
        IM_COL32(255, 255, 0, 255), 2.0f);
        
    x_pos = bar_start.x + bar_width * excess_pos;
    draw_list->AddLine(
        ImVec2(x_pos, bar_start.y), 
        ImVec2(x_pos, bar_end.y),
        IM_COL32(255, 0, 0, 255), 2.0f);

    ImGui::Spacing();
    
    // Volume threshold controls
    {
        bool auto_vol = config.auto_volume_threshold;
        if (ImGui::Checkbox("Auto##vol", &auto_vol)) {
            config.auto_volume_threshold = auto_vol;
            SaveConfiguration(); // Auto-save when auto button changes
        }
        ImGui::SameLine();
        
        ImGui::BeginDisabled(config.auto_volume_threshold);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        bool thresh_changed = ImGui::SliderFloat("Volume Threshold", &config.volume_threshold, 0.001f, 0.5f, "%.3f");
        if (thresh_changed) {
            SaveConfiguration(); // Auto-save when threshold changes
        }
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Minimum volume to trigger ear perk\nAuto mode adjusts based on ambient volume");
        }
    }
    
    // Excessive threshold controls
    {
        bool auto_excess = config.auto_excessive_threshold;
        if (ImGui::Checkbox("Auto##excess", &auto_excess)) {
            config.auto_excessive_threshold = auto_excess;
            SaveConfiguration(); // Auto-save when auto button changes
        }
        ImGui::SameLine();
        
        ImGui::BeginDisabled(config.auto_excessive_threshold);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        bool excess_changed = ImGui::SliderFloat("Excessive Volume", &config.excessive_volume_threshold, 0.05f, 1.0f, "%.3f");
        if (excess_changed) {
            SaveConfiguration(); // Auto-save when threshold changes
        }
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Volume threshold for protective ear folding\nAuto mode adjusts based on peak volumes");
        }
    }
}

void EarPerkApp::DrawStatusIndicators() {
    if (!ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const float TEXT_WIDTH = ImGui::GetContentRegionAvail().x;

    // Create static colors to avoid recreation each frame
    static const ImVec4 active_color(0.0f, 1.0f, 0.0f, 1.0f);
    static const ImVec4 inactive_color(0.5f, 0.5f, 0.5f, 1.0f);
    static const ImVec4 warning_color(1.0f, 0.0f, 0.0f, 1.0f);

    // Create a child window with fixed size
    ImGui::BeginChild("StatusChild", ImVec2(TEXT_WIDTH, 100.0f), true,
        ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoScrollbar);

    // Draw each status with proper spacing
    ImGui::Spacing();

    bool audio_working = audioProcessor && audioProcessor->IsAudioWorking();
    bool left_perked = audioProcessor && audioProcessor->IsLeftPerked();
    bool right_perked = audioProcessor && audioProcessor->IsRightPerked();
    bool overwhelmed = audioProcessor && audioProcessor->IsOverwhelmed();

    if (!audio_working) {
        ImGui::TextColored(warning_color, "Audio: Not Working");
    } else {
        ImGui::TextColored(active_color, "Audio: Working");
    }
    ImGui::Spacing();

    ImGui::TextColored(left_perked ? active_color : inactive_color, "Left Ear Perked");
    ImGui::Spacing();

    ImGui::TextColored(right_perked ? active_color : inactive_color, "Right Ear Perked");
    ImGui::Spacing();

    ImGui::TextColored(overwhelmed ? warning_color : inactive_color, "Overwhelmingly Loud");

    ImGui::EndChild();
    
    ImGui::Spacing();
    if (ImGui::Button("Reconnect Audio Device")) {
        if (audioProcessor && audioProcessor->RestartAudio()) {
            statusMessage = "Audio device reconnected successfully!";
            statusMessageTime = std::chrono::steady_clock::now();
        } else {
            statusMessage = "Failed to reconnect audio device!";
            statusMessageTime = std::chrono::steady_clock::now();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Manually restart audio processing and reconnect to the current default audio device.\nUse this if audio stops working after changing audio devices.");
    }
    
    // Show status message if recent
    if (!statusMessage.empty()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - statusMessageTime).count();
        if (elapsed < 3) { // Show for 3 seconds
            ImGui::Spacing();
            ImVec4 color = statusMessage.find("successfully") != std::string::npos ? 
                          ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            ImGui::TextColored(color, "%s", statusMessage.c_str());
        } else {
            statusMessage.clear(); // Clear after 3 seconds
        }
    }
}

void EarPerkApp::DrawAudioDeviceSelection() {
    if (!ImGui::CollapsingHeader("Audio Device Selection", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    
    if (!audioProcessor) {
        ImGui::Text("Audio processor not initialized");
        return;
    }
    
    // Show warning if audio is not working (use a stable height to prevent UI jumping)
    if (!audioProcessor->IsAudioWorking()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f)); // Orange
        ImGui::Text("⚠ Audio initialization failed - select a different device below");
        ImGui::PopStyleColor();
    } else {
        // Reserve the same space when audio is working to prevent UI jumping
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Audio is working properly");
    }
    ImGui::Spacing();
    
    // Get available devices (with caching to prevent UI flickering)
    auto now = std::chrono::steady_clock::now();
    auto timeSinceRefresh = std::chrono::duration_cast<std::chrono::seconds>(now - lastDeviceRefresh);
    
    // Refresh device list every 2 seconds or if cache is empty
    if (cachedDevices.empty() || timeSinceRefresh.count() > 2) {
        cachedDevices = audioProcessor->GetAvailableDevices();
        lastDeviceRefresh = now;
    }
    
    auto& devices = cachedDevices;
    std::string currentDeviceId = audioProcessor->GetCurrentDeviceId();
    std::string currentDeviceName = audioProcessor->GetCurrentDeviceName();
    
    // Show current device
    ImGui::Text("Current Device:");
    ImGui::Indent();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", currentDeviceName.c_str());
    ImGui::Unindent();
    ImGui::Spacing();
    
    // Device selection dropdown
    ImGui::Text("Select Audio Device:");
    
    // Create list of device names for combo
    std::vector<std::string> deviceNames;
    std::vector<std::string> deviceIds;
    int currentSelection = -1;
    
    // Add "Default Device" option
    deviceNames.push_back("Use Default Device");
    deviceIds.push_back("");
    if (config.selected_device_id.empty()) {
        currentSelection = 0;
    }
    
    // Add all available devices
    for (size_t i = 0; i < devices.size(); i++) {
        std::string displayName = devices[i].name;
        if (devices[i].isDefault) {
            displayName += " (System Default)";
        }
        deviceNames.push_back(displayName);
        deviceIds.push_back(devices[i].id);
        
        if (config.selected_device_id == devices[i].id) {
            currentSelection = static_cast<int>(i + 1);
        }
    }
    
    // Convert to char* array for ImGui
    std::vector<const char*> deviceNamesCStr;
    for (const auto& name : deviceNames) {
        deviceNamesCStr.push_back(name.c_str());
    }
    
    if (ImGui::Combo("##DeviceSelection", &currentSelection, deviceNamesCStr.data(), 
                     static_cast<int>(deviceNamesCStr.size()))) {
        if (currentSelection >= 0 && currentSelection < static_cast<int>(deviceIds.size())) {
            std::string selectedId = deviceIds[currentSelection];
            
            // Show immediate feedback
            statusMessage = "Changing audio device...";
            statusMessageTime = std::chrono::steady_clock::now();
            
            if (audioProcessor->SetSelectedDevice(selectedId)) {
                statusMessage = "Audio device changed successfully!";
                statusMessageTime = std::chrono::steady_clock::now();
                SaveConfiguration(); // Save the new device selection
            } else {
                statusMessage = "Failed to change audio device! Try a different device.";
                statusMessageTime = std::chrono::steady_clock::now();
            }
        }
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Select the audio device to capture from.\n\n"
                         "For VoiceMeeter users:\n"
                         "• VoiceMeeter Output devices (A1, B1, etc.): Mixed audio from VoiceMeeter\n"
                         "• VoiceMeeter Input devices (VAIO, AUX, etc.): Virtual microphones\n"
                         "• All VoiceMeeter devices use direct capture (no loopback needed)\n\n"
                         "Tip: Choose the VoiceMeeter output that matches your routing setup");
    }
    
    ImGui::Spacing();
    if (ImGui::Button("Refresh Device List")) {
        // Force immediate refresh of device cache
        cachedDevices = audioProcessor->GetAvailableDevices();
        lastDeviceRefresh = std::chrono::steady_clock::now();
        statusMessage = "Device list refreshed!";
        statusMessageTime = std::chrono::steady_clock::now();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Refresh the list of available audio devices.\nUse this if you've connected/disconnected audio devices.");
    }
    
    // Check if any VoiceMeeter devices are available and show troubleshooting if none found
    bool hasVoiceMeeterDevice = false;
    for (const auto& device : devices) {
        if (device.name.find("VoiceMeeter") != std::string::npos || 
            device.name.find("VAIO") != std::string::npos ||
            device.name.find("VB-Audio") != std::string::npos) {
            hasVoiceMeeterDevice = true;
            break;
        }
    }
    
    if (!hasVoiceMeeterDevice && !devices.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f)); // Orange
        ImGui::Text("⚠ No VoiceMeeter virtual devices found");
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("If you're using VoiceMeeter:\n"
                             "1. Ensure VoiceMeeter is running\n"
                             "2. Check Windows Sound settings > Recording tab\n"
                             "3. Enable VoiceMeeter VAIO/Output devices\n"
                             "4. Set a VoiceMeeter device as default recording device\n"
                             "5. Click 'Refresh Device List' after making changes");
        }
    }
}

void EarPerkApp::DrawStatusText() {
    ImGui::Separator();
    ImGui::Text("OSC Messages:");

    std::string status;
    bool left_perked = audioProcessor->IsLeftPerked();
    bool right_perked = audioProcessor->IsRightPerked();
    bool overwhelmed = audioProcessor->IsOverwhelmed();

    if (overwhelmed) {
        status += "O ";
    }
    if (left_perked && right_perked) {
        status += "B ";
    }
    else {
        if (left_perked) status += "L ";
        if (right_perked) status += "R ";
    }

    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", status.c_str());
}


void EarPerkApp::DrawConfigurationPanel() {
    if (ImGui::CollapsingHeader("Advanced Configuration")) {
        ImGui::Text("OSC Settings");

        char addr_buf[256];
        strncpy(addr_buf, config.address.c_str(), sizeof(addr_buf) - 1);
        if (ImGui::InputText("Address", addr_buf, sizeof(addr_buf))) {
            config.address = addr_buf;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("IP address to send OSC messages to (usually 127.0.0.1 for local VRChat)");
        }

        int port = config.port;
        if (ImGui::InputInt("Port", &port)) {
            config.port = port;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Port number for OSC messages (usually 9000 for VRChat)");
        }

        ImGui::Separator();
        ImGui::Text("Logging");

        // Log Level dropdown
        const char* logLevelItems[] = { "DEBUG", "INFO", "WARN", "ERROR" };
        int currentLogLevel = static_cast<int>(config.log_level);
        if (ImGui::Combo("Log Level", &currentLogLevel, logLevelItems, 4)) {
            config.log_level = static_cast<LogLevel>(currentLogLevel);
            Logger::getInstance().SetLevel(config.log_level);
            SaveConfiguration(); // Auto-save when log level changes
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Set the minimum logging level\nDEBUG: Most verbose\nINFO: General information\nWARN: Warnings and errors (recommended)\nERROR: Only errors");
        }

        ImGui::Separator();
        ImGui::Text("Thresholds");

        bool changed = false;
        float differential = config.differential_threshold;

        if (ImGui::SliderFloat("Differential Threshold", &differential, 0.001f, 0.1f, "%.3f")) {
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Minimum difference in volume between ears to trigger only one to perk");
        }

        int timeout = config.timeout_ms;
        if (ImGui::SliderInt("Cooldown Time", &timeout, 50, 1000, "%d ms")) {
            config.timeout_ms = timeout;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Minimum time between ear perks");
        }

        int reset_timeout = config.reset_timeout_ms;
        if (ImGui::SliderInt("Reset Time", &reset_timeout, 500, 5000, "%d ms")) {
            config.reset_timeout_ms = reset_timeout;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Time until ears return to neutral position after being perked");
        }

        if (ImGui::TreeNode("Auto Threshold Settings")) {
            bool changed = false;
            
            changed |= ImGui::SliderFloat("Volume Threshold Multiplier", 
                &config.volume_threshold_multiplier, 1.0f, 4.0f, "%.1f std dev");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("How many standard deviations above mean\nfor auto volume threshold");
            }

            changed |= ImGui::SliderFloat("Excessive Threshold Multiplier",
                &config.excessive_threshold_multiplier, 2.0f, 5.0f, "%.1f std dev");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("How many standard deviations above mean\nfor auto excessive threshold");
            }

            if (changed) {
                SaveConfiguration(); // Auto-save when multipliers change
            }

            ImGui::TreePop();
        }

        if (changed) {
            UpdateThresholds(differential, config.volume_threshold, config.excessive_volume_threshold);
        }

        if (ImGui::Button("Save Configuration")) {
            SaveConfiguration();
        }
        ImGui::SameLine();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Save current settings to config.ini");
        }
    }
}

void EarPerkApp::SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Window styling
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;

    // Color scheme
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.37f, 0.37f, 0.37f, 0.40f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.39f, 0.39f, 0.39f, 0.67f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.37f, 0.37f, 0.37f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);

    // Spacing and sizes
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.FramePadding = ImVec2(4.0f, 3.0f);
}

void EarPerkApp::UpdateThresholds(float differential, float volume, float excessive) {
    config.differential_threshold = differential;
    config.volume_threshold = volume;
    config.excessive_volume_threshold = excessive;

    // Update audio processor with new thresholds
    if (audioProcessor) {
        audioProcessor->UpdateThresholds(differential, volume, excessive);
    }
}

void EarPerkApp::SaveConfiguration() {
    LOG_DEBUG_F("Saving configuration with selected device ID: '%s'", config.selected_device_id.c_str());
    bool saved = config.SaveToFile();
    if (saved) {
        LOG_DEBUG("Configuration saved successfully");
    } else {
        LOG_ERROR("Failed to save configuration");
    }
}

void EarPerkApp::WindowFocusCallback(GLFWwindow* window, int focused) {
    auto* app = static_cast<EarPerkApp*>(glfwGetWindowUserPointer(window));
    if (focused && app->wasMinimized) {
        // Reinitialize ImGui safely
        if (ImGui::GetCurrentContext()) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");
        
        // Reapply the ImGui style
        app->SetupImGuiStyle();

        app->wasMinimized = false;
    }
}

void EarPerkApp::WindowIconifyCallback(GLFWwindow* window, int iconified) {
    auto* app = static_cast<EarPerkApp*>(glfwGetWindowUserPointer(window));
    app->wasMinimized = iconified != 0;
}

void EarPerkApp::SetWindowIcon() {
#ifdef _WIN32
    // Get the window handle
    HWND hwnd = glfwGetWin32Window(window);
    
    // Load icon from resources
    HICON hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(100));
    if (hIcon) {
        // Set both small and large icons
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }
#endif
}
