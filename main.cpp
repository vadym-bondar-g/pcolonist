#include "pcolonist/core/Application.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    try {
        pcolonist::Application application;
        application.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
