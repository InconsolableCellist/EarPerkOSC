#include "app.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <glad/glad.h>

EarPerkApp::EarPerkApp() : window(nullptr) {}

EarPerkApp::~EarPerkApp() {
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
    audioProcessor = std::make_unique<AudioProcessor>(config);
    if (!audioProcessor->Initialize()) {
        std::cerr << "Failed to initialize audio processor" << std::endl;
        return false;
    }

    audioProcessor->Start();
    return true;
}

void EarPerkApp::Run() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

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

    ImGui::Begin("EarPerk OSC");

    DrawVolumeMeters();
    ImGui::Separator();
    DrawStatusIndicators();
    DrawConfigurationPanel();

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EarPerkApp::DrawVolumeMeters() {
    ImGui::Text("Volume Levels");

    float left_vol = audioProcessor->GetLeftVolume();
    float right_vol = audioProcessor->GetRightVolume();

    // Scale volumes for display
    const float scale = 2.0f; // Adjust this value to make meters more visible
    left_vol *= scale;
    right_vol *= scale;

    ImGui::ProgressBar(left_vol, ImVec2(-1, 0), "Left Channel");
    ImGui::ProgressBar(right_vol, ImVec2(-1, 0), "Right Channel");
}

void EarPerkApp::DrawStatusIndicators() {
    ImGui::Text("Status");
    ImGui::BeginChild("Status", ImVec2(0, 60), true);

    ImVec4 active_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    ImVec4 inactive_color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImVec4 warning_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

    ImGui::TextColored(audioProcessor->IsLeftPerked() ? active_color : inactive_color,
        "Left Ear Perked");
    ImGui::TextColored(audioProcessor->IsRightPerked() ? active_color : inactive_color,
        "Right Ear Perked");
    ImGui::TextColored(audioProcessor->IsOverwhelmed() ? warning_color : inactive_color,
        "Overwhelmingly Loud");

    ImGui::EndChild();
}

void EarPerkApp::DrawConfigurationPanel() {
    if (ImGui::CollapsingHeader("Configuration")) {
        ImGui::Text("OSC Settings");
        ImGui::Text("Address: %s", config.address.c_str());
        ImGui::Text("Port: %d", config.port);

        ImGui::Separator();
        ImGui::Text("Thresholds");
        ImGui::Text("Differential: %.3f", config.differential_threshold);
        ImGui::Text("Volume: %.3f", config.volume_threshold);
        ImGui::Text("Excessive: %.3f", config.excessive_volume_threshold);
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