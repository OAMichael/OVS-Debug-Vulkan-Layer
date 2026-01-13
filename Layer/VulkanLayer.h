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

    Count
};

static inline const char* GetLayerTypeName(VulkanLayerType type) {
    switch (type) {
        case VulkanLayerType::PassThrough:      return "PassThrough";
        case VulkanLayerType::TerminatorBase:   return "TerminatorBase";
        case VulkanLayerType::Terminator:       return "Terminator";
        case VulkanLayerType::Printer:          return "Printer";
        case VulkanLayerType::Screenshot:       return "Screenshot";
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
    return VulkanLayerType::None;
}

class VulkanLayerInterface;
using VulkanLayerPtr = std::unique_ptr<VulkanLayerInterface>;

struct VulkanLayerPrinterSettings {
    std::string filename;
};

struct VulkanLayerScreenshotSettings {
    std::string fileBaseName;
    std::vector<FrameRange> frameRanges;
};

} // namespace OVS

#endif // LAYER__VULKAN_LAYER_H