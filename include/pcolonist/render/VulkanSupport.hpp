#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace pcolonist {

struct VulkanDeviceInfo {
    std::string name;
    std::string type;
    std::string apiVersion;
    std::string driverVersion;
    bool graphicsQueue = false;
    bool presentQueue = false;
};

struct VulkanProbeResult {
    bool compiled = false;
    bool available = false;
    std::string message;
    std::vector<std::string> instanceExtensions;
    std::vector<VulkanDeviceInfo> devices;
};

[[nodiscard]] VulkanProbeResult probeVulkanSupport();
void printVulkanProbe(std::ostream& output, const VulkanProbeResult& probe);

} // namespace pcolonist
