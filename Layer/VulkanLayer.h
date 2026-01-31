#ifndef LAYER__VULKAN_LAYER_H
#define LAYER__VULKAN_LAYER_H

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace OVS {

struct FrameRange {
    uint32_t start{0};
    uint32_t end{0};
};

enum class VulkanLayerType {
    None = 0,
    PassThrough,
    TerminatorBase,
    Terminator,
    Printer,
    Screenshot,
    APIDump,
    APITrace,

    Count
};

static inline const char* GetLayerTypeName(VulkanLayerType type) {
    switch (type) {
        case VulkanLayerType::PassThrough:      return "PassThrough";
        case VulkanLayerType::TerminatorBase:   return "TerminatorBase";
        case VulkanLayerType::Terminator:       return "Terminator";
        case VulkanLayerType::Printer:          return "Printer";
        case VulkanLayerType::Screenshot:       return "Screenshot";
        case VulkanLayerType::APIDump:          return "APIDump";
        case VulkanLayerType::APITrace:         return "APITrace";
        default:                                return "Unknown";
    }
}

static inline VulkanLayerType GetLayerTypeByName(std::string_view name) {
    if (name == "PassThrough") {
        return VulkanLayerType::PassThrough;
    }
    if (name == "TerminatorBase") {
        return VulkanLayerType::TerminatorBase;
    }
    if (name == "Terminator") {
        return VulkanLayerType::Terminator;
    }
    if (name == "Printer") {
        return VulkanLayerType::Printer;
    }
    if (name == "Screenshot") {
        return VulkanLayerType::Screenshot;
    }
    if (name == "APIDump") {
        return VulkanLayerType::APIDump;
    }
    if (name == "APITrace") {
        return VulkanLayerType::APITrace;
    }
    return VulkanLayerType::None;
}

class VulkanLayerInterface;
using VulkanLayerPtr = std::unique_ptr<VulkanLayerInterface>;

constexpr const char PrinterDefaultFilename[] = "stdout";
constexpr const char ScreenshotDefaultFileBaseName[] = "screenshot";
constexpr const char APIDumpDefaultFilename[] = "stdout";
constexpr const char APITraceDefaultFilename[] = "apitrace.ovs";
constexpr size_t APITraceDefaultFlushSize = 200 * 1024 * 1024;

struct VulkanLayerPrinterSettings {
    std::string filename{PrinterDefaultFilename};
};

struct VulkanLayerScreenshotSettings {
    std::string fileBaseName{ScreenshotDefaultFileBaseName};
    std::vector<FrameRange> frameRanges;
};

struct VulkanLayerAPIDumpSettings {
    std::string filename{APIDumpDefaultFilename};
};

struct VulkanLayerAPITraceSettings {
    std::string filename{APITraceDefaultFilename};
    size_t flushSize{APITraceDefaultFlushSize};
};

} // namespace OVS

#endif // LAYER__VULKAN_LAYER_H