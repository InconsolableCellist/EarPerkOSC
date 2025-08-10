#pragma once

// Windows-specific definitions
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <memory>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "audio_processor.hpp"
#include "config.hpp"

class EarPerkApp {
public:
    EarPerkApp();
    ~EarPerkApp();

    // Delete copy constructor and assignment operator
    EarPerkApp(const EarPerkApp&) = delete;
    EarPerkApp& operator=(const EarPerkApp&) = delete;

    bool Initialize();
    void Run();

private:
    void RenderUI();
    void SetupImGuiStyle();
    void DrawVolumeMeters();
    void DrawStatusIndicators();
    void DrawConfigurationPanel();
    void UpdateThresholds(float differential, float volume, float excessive);
    void SaveConfiguration();
    void DrawStatusText();

    GLFWwindow* window;
    std::unique_ptr<AudioProcessor> audioProcessor;
    Config config;

    // Window settings
    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;
    const char* WINDOW_TITLE = "EarPerk OSC";
    bool wasMinimized = false;

    static void WindowFocusCallback(GLFWwindow* window, int focused);
    static void WindowIconifyCallback(GLFWwindow* window, int iconified);
    void SetWindowIcon();
};