#include "app.hpp"
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

EarPerkApp::EarPerkApp() : window(nullptr) {
	ImGui::SetCurrentContext(nullptr);  // Ensure no context exists before creation
}

EarPerkApp::~EarPerkApp() {
    // Save configuration before shutting down
    SaveConfiguration();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

bool EarPerkApp::Initialize() {
    // Load configuration
    if (!config.LoadFromFile()) {
        std::cerr << "Failed to load configuration" << std::endl;
        return false;
    }

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Create window with graphics context
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }

    // Set window icon
    SetWindowIcon();

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    glfwSwapInterval(1); // Enable vsync

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    SetupImGuiStyle();

    // Initialize audio processor
    audioProcessor = std::make_unique<AudioProcessor>(std::ref(config));
    if (!audioProcessor->Initialize()) {
        std::cerr << "Failed to initialize audio processor" << std::endl;
        return false;
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetWindowFocusCallback(window, WindowFocusCallback);
    glfwSetWindowIconifyCallback(window, WindowIconifyCallback);

    audioProcessor->Start();
    return true;
}

void EarPerkApp::Run() {
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
}

void EarPerkApp::RenderUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Set up main window with proper flags
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoTitleBar | // Add this to remove title bar
        ImGuiWindowFlags_NoBringToFrontOnFocus; // Add this to prevent window from floating

    // Get the GLFW window size
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Set the window position to (0,0) and size to full window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGui::Begin("EarPerk OSC", nullptr, window_flags);

    DrawVolumeMeters();
    ImGui::Separator();
    DrawStatusIndicators();
    DrawConfigurationPanel();
    DrawStatusText();

    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 25);
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "EarPerkOSC v1.0 by Foxipso - foxipso.com");

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
    ImGui::ProgressBar(left_vol, ImVec2(-1, 20));
    
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
    ImGui::ProgressBar(right_vol, ImVec2(-1, 20));
    
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
    ImGui::BeginChild("StatusChild", ImVec2(TEXT_WIDTH, 80), true,
        ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoScrollbar);

    // Draw each status with proper spacing
    ImGui::Spacing();

    bool left_perked = audioProcessor && audioProcessor->IsLeftPerked();
    bool right_perked = audioProcessor && audioProcessor->IsRightPerked();
    bool overwhelmed = audioProcessor && audioProcessor->IsOverwhelmed();

    ImGui::TextColored(left_perked ? active_color : inactive_color, "Left Ear Perked");
    ImGui::Spacing();

    ImGui::TextColored(right_perked ? active_color : inactive_color, "Right Ear Perked");
    ImGui::Spacing();

    ImGui::TextColored(overwhelmed ? warning_color : inactive_color, "Overwhelmingly Loud");

    ImGui::EndChild();
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
    style.ItemSpacing = ImVec2(8, 4);
    style.FramePadding = ImVec2(4, 3);
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
    config.SaveToFile();
}

void EarPerkApp::WindowFocusCallback(GLFWwindow* window, int focused) {
    auto* app = static_cast<EarPerkApp*>(glfwGetWindowUserPointer(window));
    if (focused && app->wasMinimized) {
        // Reinitialize ImGui
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

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
