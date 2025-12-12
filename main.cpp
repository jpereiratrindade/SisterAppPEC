#include "core/application.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    try {
        core::Application app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Application Error: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown Fatal Error" << std::endl;
        return -1;
    }
    return 0;
}
