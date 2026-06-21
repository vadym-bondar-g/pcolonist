#include "pcolonist/core/Application.hpp"
#include "pcolonist/core/RuntimeOptions.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> arguments(int argc, char** argv) {
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        result.emplace_back(argv[index]);
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const pcolonist::RuntimeOptions options = pcolonist::parseRuntimeOptions(arguments(argc, argv));
        switch (options.mode) {
        case pcolonist::LaunchMode::Help:
            pcolonist::printUsage(std::cout, argc > 0 ? argv[0] : "pcolonist");
            return static_cast<int>(pcolonist::ExitCode::Success);
        case pcolonist::LaunchMode::Version:
            pcolonist::printVersion(std::cout);
            return static_cast<int>(pcolonist::ExitCode::Success);
        case pcolonist::LaunchMode::ValidateAssets:
            return pcolonist::validateRuntimeAssets(options.application.assetRoot, std::cout)
                ? static_cast<int>(pcolonist::ExitCode::Success)
                : static_cast<int>(pcolonist::ExitCode::AssetValidationFailed);
        case pcolonist::LaunchMode::Run:
            break;
        }

        std::clog << "pcolonist: starting with assets=" << options.application.assetRoot
                  << " window=" << options.application.windowWidth << 'x' << options.application.windowHeight
                  << " vsync=" << (options.application.vsync ? "on" : "off") << '\n';
        pcolonist::Application application(options.application);
        application.initialize();
        application.run();
        application.shutdown();
    } catch (const pcolonist::ArgumentError& error) {
        std::cerr << "pcolonist: invalid arguments: " << error.what() << '\n';
        pcolonist::printUsage(std::cerr, argc > 0 ? argv[0] : "pcolonist");
        return static_cast<int>(pcolonist::ExitCode::InvalidArguments);
    } catch (const std::exception& error) {
        std::cerr << "pcolonist: fatal: " << error.what() << '\n';
        return static_cast<int>(pcolonist::ExitCode::RuntimeError);
    } catch (...) {
        std::cerr << "pcolonist: fatal: unknown exception\n";
        return static_cast<int>(pcolonist::ExitCode::RuntimeError);
    }

    return static_cast<int>(pcolonist::ExitCode::Success);
}
