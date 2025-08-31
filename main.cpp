#include "app.hpp"
#include "logger.hpp"
#include <iostream>
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // First, output to console that we're starting up
    std::cout << "EarPerkOSC starting up..." << std::endl;
    
    bool loggerInitialized = false;
    try {
        Logger& logger = Logger::getInstance();
        loggerInitialized = logger.Initialize();
        if (!loggerInitialized) {
            std::cerr << "Warning: Logger initialization failed, continuing without file logging" << std::endl;
        } else {
            std::cout << "Logger initialized successfully" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during logger initialization: " << e.what() << std::endl;
        loggerInitialized = false;
    }
    catch (...) {
        std::cerr << "Unknown exception during logger initialization" << std::endl;
        loggerInitialized = false;
    }
    
    if (loggerInitialized) {
        LOG_INFO("=== EarPerkOSC Application Starting ===");
        LOG_INFO_F("Command line: %s", lpCmdLine ? lpCmdLine : "");
        LOG_INFO_F("Show command: %d", nCmdShow);
    } else {
        std::cout << "=== EarPerkOSC Application Starting (no file logging) ===" << std::endl;
    }
    
    try {
        if (loggerInitialized) {
            LOG_INFO("Creating EarPerkApp instance");
        }
        std::cout << "Creating EarPerkApp instance..." << std::endl;
        EarPerkApp app;

        if (loggerInitialized) {
            LOG_INFO("Initializing application");
        }
        std::cout << "Initializing application..." << std::endl;
        
        bool initResult = false;
        try {
            initResult = app.Initialize();
        } catch (const std::exception& e) {
            if (loggerInitialized) {
                LOG_ERROR_F("Exception during app initialization: %s", e.what());
                Logger::getInstance().Flush();
            }
            std::cerr << "Exception during app initialization: " << e.what() << std::endl;
            return -1;
        } catch (...) {
            if (loggerInitialized) {
                LOG_ERROR("Unknown exception during app initialization");
                Logger::getInstance().Flush();
            }
            std::cerr << "Unknown exception during app initialization" << std::endl;
            return -1;
        }
        
        if (!initResult) {
            if (loggerInitialized) {
                LOG_ERROR("Failed to initialize application");
                Logger::getInstance().Flush();
            }
            std::cerr << "Failed to initialize application" << std::endl;
            return -1;
        }

        if (loggerInitialized) {
            LOG_INFO("Application initialized successfully, starting main loop");
        }
        std::cout << "Application initialized successfully, starting main loop..." << std::endl;
        
        try {
            app.Run();
        } catch (const std::exception& e) {
            if (loggerInitialized) {
                LOG_ERROR_F("Exception during app.Run(): %s", e.what());
                Logger::getInstance().Flush();
            }
            std::cerr << "Exception during app.Run(): " << e.what() << std::endl;
            return -1;
        } catch (...) {
            if (loggerInitialized) {
                LOG_ERROR("Unknown exception during app.Run()");
                Logger::getInstance().Flush();
            }
            std::cerr << "Unknown exception during app.Run()" << std::endl;
            return -1;
        }
        
        if (loggerInitialized) {
            LOG_INFO("Application main loop completed normally");
            Logger::getInstance().Flush();
        }
        std::cout << "Application completed normally" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        if (loggerInitialized) {
            LOG_ERROR_F("Fatal exception caught: %s", e.what());
            Logger::getInstance().Flush();
        }
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
    catch (...) {
        if (loggerInitialized) {
            LOG_ERROR("Unknown fatal exception caught");
            Logger::getInstance().Flush();
        }
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return -1;
    }
}