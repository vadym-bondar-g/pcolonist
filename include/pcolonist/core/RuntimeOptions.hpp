#pragma once

#include "pcolonist/core/ApplicationConfig.hpp"

#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

namespace pcolonist {

enum class LaunchMode {
    Run,
    Help,
    Version,
    ValidateAssets,
    DumpConfig,
    ProbeVulkan,
};

enum class ExitCode : int {
    Success = 0,
    RuntimeError = 1,
    InvalidArguments = 2,
    AssetValidationFailed = 3,
};

struct RuntimeOptions {
    LaunchMode mode = LaunchMode::Run;
    ApplicationConfig application;
};

class ArgumentError final : public std::runtime_error {
public:
    explicit ArgumentError(const std::string& message);
};

[[nodiscard]] RuntimeOptions parseRuntimeOptions(const std::vector<std::string>& arguments);
void printUsage(std::ostream& output, const char* executable);
void printVersion(std::ostream& output);
[[nodiscard]] bool validateRuntimeAssets(const std::filesystem::path& assetRoot, std::ostream& output);

} // namespace pcolonist
