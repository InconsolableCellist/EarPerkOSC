#include "app.hpp"
#include <iostream>

int main() {
    try {
        EarPerkApp app;

        if (!app.Initialize()) {
            std::cerr << "Failed to initialize application" << std::endl;
            return -1;
        }

        app.Run();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }
    catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return -1;
    }
}