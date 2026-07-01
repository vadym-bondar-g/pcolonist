#include "pcolonist/render/VulkanSupport.hpp"

#include <algorithm>
#include <cstdint>
#include <dlfcn.h>
#include <ostream>
#include <string>
#include <vector>

namespace pcolonist {

namespace {

#if PCOLONIST_ENABLE_VULKAN

using VkFlags = std::uint32_t;
using VkResult = std::int32_t;
using VkStructureType = std::int32_t;
using VkInstance = struct VkInstance_T*;
using VkPhysicalDevice = struct VkPhysicalDevice_T*;

constexpr VkResult vkSuccess = 0;
constexpr VkStructureType vkStructureTypeApplicationInfo = 0;
constexpr VkStructureType vkStructureTypeInstanceCreateInfo = 1;
constexpr std::uint32_t vkApiVersion10 = 1U << 22U;
constexpr VkFlags vkQueueGraphicsBit = 0x00000001U;
constexpr std::size_t vkMaxExtensionNameSize = 256;

struct VkApplicationInfo {
    VkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    std::uint32_t applicationVersion;
    const char* pEngineName;
    std::uint32_t engineVersion;
    std::uint32_t apiVersion;
};

struct VkInstanceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    const VkApplicationInfo* pApplicationInfo;
    std::uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    std::uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

struct VkAllocationCallbacks;

struct VkExtensionProperties {
    char extensionName[vkMaxExtensionNameSize];
    std::uint32_t specVersion;
};

struct VkExtent3D {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
};

struct VkQueueFamilyProperties {
    VkFlags queueFlags;
    std::uint32_t queueCount;
    std::uint32_t timestampValidBits;
    VkExtent3D minImageTransferGranularity;
};

using PfnVoidFunction = void (*)();
using PfnVkGetInstanceProcAddr = PfnVoidFunction (*)(VkInstance, const char*);
using PfnVkCreateInstance = VkResult (*)(
    const VkInstanceCreateInfo*,
    const VkAllocationCallbacks*,
    VkInstance*);
using PfnVkDestroyInstance = void (*)(VkInstance, const VkAllocationCallbacks*);
using PfnVkEnumerateInstanceExtensionProperties = VkResult (*)(
    const char*,
    std::uint32_t*,
    VkExtensionProperties*);
using PfnVkEnumeratePhysicalDevices = VkResult (*)(
    VkInstance,
    std::uint32_t*,
    VkPhysicalDevice*);
using PfnVkGetPhysicalDeviceQueueFamilyProperties = void (*)(
    VkPhysicalDevice,
    std::uint32_t*,
    VkQueueFamilyProperties*);

struct VulkanLoader {
    void* library = nullptr;
    PfnVkGetInstanceProcAddr getInstanceProcAddr = nullptr;

    VulkanLoader() {
        for (const char* name : {"libvulkan.so.1", "libvulkan.so"}) {
            library = dlopen(name, RTLD_NOW | RTLD_LOCAL);
            if (library != nullptr) {
                break;
            }
        }
        if (library != nullptr) {
            getInstanceProcAddr = reinterpret_cast<PfnVkGetInstanceProcAddr>(dlsym(library, "vkGetInstanceProcAddr"));
        }
    }

    ~VulkanLoader() {
        if (library != nullptr) {
            dlclose(library);
        }
    }

    VulkanLoader(const VulkanLoader&) = delete;
    VulkanLoader& operator=(const VulkanLoader&) = delete;
};

template <typename Function>
Function loadGlobal(const VulkanLoader& loader, const char* name) {
    return loader.getInstanceProcAddr != nullptr
        ? reinterpret_cast<Function>(loader.getInstanceProcAddr(nullptr, name))
        : nullptr;
}

template <typename Function>
Function loadInstance(const VulkanLoader& loader, VkInstance instance, const char* name) {
    return loader.getInstanceProcAddr != nullptr
        ? reinterpret_cast<Function>(loader.getInstanceProcAddr(instance, name))
        : nullptr;
}

std::string resultName(VkResult result) {
    switch (result) {
    case vkSuccess:
        return "VK_SUCCESS";
    case 1:
        return "VK_NOT_READY";
    case 2:
        return "VK_TIMEOUT";
    case 3:
        return "VK_EVENT_SET";
    case 4:
        return "VK_EVENT_RESET";
    case 5:
        return "VK_INCOMPLETE";
    case -1:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case -2:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case -3:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case -4:
        return "VK_ERROR_DEVICE_LOST";
    case -5:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case -6:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case -7:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case -8:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case -9:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    default:
        return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
    }
}

bool hasGraphicsQueue(PfnVkGetPhysicalDeviceQueueFamilyProperties getQueues, VkPhysicalDevice device) {
    std::uint32_t queueFamilyCount = 0;
    getQueues(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queues(queueFamilyCount);
    getQueues(device, &queueFamilyCount, queues.data());
    return std::any_of(queues.begin(), queues.end(), [](const VkQueueFamilyProperties& queue) {
        return queue.queueCount > 0 && (queue.queueFlags & vkQueueGraphicsBit) != 0;
    });
}

#endif

} // namespace

VulkanProbeResult probeVulkanSupport() {
    VulkanProbeResult probe;

#if !PCOLONIST_ENABLE_VULKAN
    probe.message = "Vulkan support was disabled at build time";
    return probe;
#else
    probe.compiled = true;

    const VulkanLoader loader;
    if (loader.library == nullptr || loader.getInstanceProcAddr == nullptr) {
        probe.message = "Vulkan loader library was not found";
        return probe;
    }
    const auto enumerateInstanceExtensions = loadGlobal<PfnVkEnumerateInstanceExtensionProperties>(
        loader,
        "vkEnumerateInstanceExtensionProperties");
    const auto createInstance = loadGlobal<PfnVkCreateInstance>(loader, "vkCreateInstance");
    if (enumerateInstanceExtensions == nullptr || createInstance == nullptr) {
        probe.message = "Vulkan loader does not expose required global entry points";
        return probe;
    }

    std::uint32_t extensionCount = 0;
    VkResult result = enumerateInstanceExtensions(nullptr, &extensionCount, nullptr);
    if (result != vkSuccess) {
        probe.message = "vkEnumerateInstanceExtensionProperties failed: " + resultName(result);
        return probe;
    }
    std::vector<VkExtensionProperties> extensions(extensionCount);
    result = enumerateInstanceExtensions(nullptr, &extensionCount, extensions.data());
    if (result != vkSuccess) {
        probe.message = "vkEnumerateInstanceExtensionProperties failed: " + resultName(result);
        return probe;
    }
    probe.instanceExtensions.reserve(extensions.size());
    for (const VkExtensionProperties& extension : extensions) {
        probe.instanceExtensions.emplace_back(extension.extensionName);
    }
    std::sort(probe.instanceExtensions.begin(), probe.instanceExtensions.end());

    const VkApplicationInfo applicationInfo{
        vkStructureTypeApplicationInfo,
        nullptr,
        "pcolonist",
        1,
        "pcolonist",
        1,
        vkApiVersion10,
    };
    const VkInstanceCreateInfo createInfo{
        vkStructureTypeInstanceCreateInfo,
        nullptr,
        0,
        &applicationInfo,
        0,
        nullptr,
        0,
        nullptr,
    };

    VkInstance instance = nullptr;
    result = createInstance(&createInfo, nullptr, &instance);
    if (result != vkSuccess) {
        probe.message = "vkCreateInstance failed: " + resultName(result);
        return probe;
    }
    const auto destroyInstance = loadInstance<PfnVkDestroyInstance>(loader, instance, "vkDestroyInstance");
    const auto enumeratePhysicalDevices = loadInstance<PfnVkEnumeratePhysicalDevices>(
        loader,
        instance,
        "vkEnumeratePhysicalDevices");
    const auto getQueueFamilyProperties = loadInstance<PfnVkGetPhysicalDeviceQueueFamilyProperties>(
        loader,
        instance,
        "vkGetPhysicalDeviceQueueFamilyProperties");
    if (destroyInstance == nullptr || enumeratePhysicalDevices == nullptr || getQueueFamilyProperties == nullptr) {
        probe.message = "Vulkan instance does not expose required device entry points";
        if (destroyInstance != nullptr) {
            destroyInstance(instance, nullptr);
        }
        return probe;
    }

    std::uint32_t deviceCount = 0;
    result = enumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (result != vkSuccess) {
        destroyInstance(instance, nullptr);
        probe.message = "vkEnumeratePhysicalDevices failed: " + resultName(result);
        return probe;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = enumeratePhysicalDevices(instance, &deviceCount, devices.data());
    if (result != vkSuccess) {
        destroyInstance(instance, nullptr);
        probe.message = "vkEnumeratePhysicalDevices failed: " + resultName(result);
        return probe;
    }

    probe.devices.reserve(devices.size());
    for (std::size_t index = 0; index < devices.size(); ++index) {
        probe.devices.push_back({
            "physical_device_" + std::to_string(index),
            "unknown",
            "unknown",
            "unknown",
            hasGraphicsQueue(getQueueFamilyProperties, devices[index]),
            false,
        });
    }

    destroyInstance(instance, nullptr);
    probe.available = !probe.devices.empty();
    probe.message = probe.available ? "Vulkan is available" : "No Vulkan physical devices found";
    return probe;
#endif
}

void printVulkanProbe(std::ostream& output, const VulkanProbeResult& probe) {
    output << "compiled=" << (probe.compiled ? "true" : "false") << '\n'
           << "available=" << (probe.available ? "true" : "false") << '\n'
           << "message=" << probe.message << '\n'
           << "instanceExtensions=" << probe.instanceExtensions.size() << '\n';
    for (const std::string& extension : probe.instanceExtensions) {
        output << "  " << extension << '\n';
    }
    output << "devices=" << probe.devices.size() << '\n';
    for (const VulkanDeviceInfo& device : probe.devices) {
        output << "  name=" << device.name << '\n'
               << "    type=" << device.type << '\n'
               << "    apiVersion=" << device.apiVersion << '\n'
               << "    driverVersion=" << device.driverVersion << '\n'
               << "    graphicsQueue=" << (device.graphicsQueue ? "true" : "false") << '\n'
               << "    presentQueue=" << (device.presentQueue ? "true" : "false") << '\n';
    }
}

} // namespace pcolonist
