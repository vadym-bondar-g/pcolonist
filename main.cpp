#include "pcolonist/core/Application.hpp"
#include "pcolonist/core/RuntimeOptions.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> arguments(int argc, char** argv) {
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        result.emplace_back(argv != nullptr && argv[index] != nullptr ? argv[index] : "");
    }
    return result;
}

const char* executableName(const std::vector<std::string>& arguments) {
    return !arguments.empty() && !arguments.front().empty() ? arguments.front().c_str() : "pcolonist";
}

void printRuntimeConfig(const pcolonist::RuntimeOptions& options, std::ostream& output) {
    output << "assetRoot=" << options.application.assetRoot.generic_string() << '\n'
           << "savePath=" << options.application.savePath.generic_string() << '\n'
           << "loadGame=" << (options.application.loadGame ? "true" : "false") << '\n'
           << "windowWidth=" << options.application.windowWidth << '\n'
           << "windowHeight=" << options.application.windowHeight << '\n'
           << "vsync=" << (options.application.vsync ? "true" : "false") << '\n'
           << "language=" << pcolonist::languageCode(options.application.language) << '\n';
}

int runApplication(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& error) {
    try {
        const pcolonist::RuntimeOptions options = pcolonist::parseRuntimeOptions(arguments);
        switch (options.mode) {
        case pcolonist::LaunchMode::Help:
            pcolonist::printUsage(output, executableName(arguments));
            return static_cast<int>(pcolonist::ExitCode::Success);
        case pcolonist::LaunchMode::Version:
            pcolonist::printVersion(output);
            return static_cast<int>(pcolonist::ExitCode::Success);
        case pcolonist::LaunchMode::ValidateAssets:
            return pcolonist::validateRuntimeAssets(options.application.assetRoot, output)
                ? static_cast<int>(pcolonist::ExitCode::Success)
                : static_cast<int>(pcolonist::ExitCode::AssetValidationFailed);
        case pcolonist::LaunchMode::DumpConfig:
            printRuntimeConfig(options, output);
            return static_cast<int>(pcolonist::ExitCode::Success);
        case pcolonist::LaunchMode::Run:
            break;
        }

        std::clog << "pcolonist: starting with assets=" << options.application.assetRoot
                  << " window=" << options.application.windowWidth << 'x' << options.application.windowHeight
                  << " vsync=" << (options.application.vsync ? "on" : "off") << '\n';
        pcolonist::Application application(options.application);
        application.initialize();
        application.run();
        std::clog << "pcolonist: exited normally\n";
    } catch (const pcolonist::ArgumentError& exception) {
        error << "pcolonist: invalid arguments: " << exception.what() << '\n';
        pcolonist::printUsage(error, executableName(arguments));
        return static_cast<int>(pcolonist::ExitCode::InvalidArguments);
    } catch (const std::exception& exception) {
        error << "pcolonist: fatal: " << exception.what() << '\n';
        return static_cast<int>(pcolonist::ExitCode::RuntimeError);
    } catch (...) {
        error << "pcolonist: fatal: unknown exception\n";
        return static_cast<int>(pcolonist::ExitCode::RuntimeError);
    }

    return static_cast<int>(pcolonist::ExitCode::Success);
}

} // namespace

int main(int argc, char** argv) {
    return runApplication(arguments(argc, argv), std::cout, std::cerr);
}
