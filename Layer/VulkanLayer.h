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

enum VulkanLayerType {
    None = 0,
    PassThrough,
    TerminatorBase,
    Terminator,
    Printer,
    Screenshot,

    Count
};

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