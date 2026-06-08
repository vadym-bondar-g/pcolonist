#include "pcolonist/core/Application.hpp"

#include <exception>
#include <iostream>

int main() {
    try {
        pcolonist::Application application;
        application.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
