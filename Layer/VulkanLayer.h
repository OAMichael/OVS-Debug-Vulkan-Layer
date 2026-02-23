#ifndef LAYER__VULKAN_LAYER_H
#define LAYER__VULKAN_LAYER_H

#include <VulkanShader.h>
#include <CommonUtils.h>

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
    ShaderProfiler,

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
        case VulkanLayerType::ShaderProfiler:   return "ShaderProfiler";
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
    if (name == "ShaderProfiler") {
        return VulkanLayerType::ShaderProfiler;
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
constexpr const char ShaderProfilerDefaultFilename[] = "shaderprof.ovs";

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

struct VulkanLayerShaderProfilerSettings {
    std::string filename{ShaderProfilerDefaultFilename};
};

constexpr uint32_t OVSFileMagic = (uint32_t('#') << 24) | (uint32_t('O') << 16) | (uint32_t('V') << 8) | (uint32_t('S'));
constexpr uint32_t OVSFileVersion = (uint32_t(1) << 24) | (uint32_t(0));

struct OVSFileHeader {
    uint32_t magic{OVSFileMagic};
    uint32_t version{OVSFileVersion};
    uint32_t layerType{uint32_t(VulkanLayerType::None)};
    uint32_t reserved{0};
};

struct APITraceFileHeader {
    uint64_t signatureCount{0};
};

struct ShaderProfilerFileHeader {
    uint64_t byteSize{0};
};

struct CollectedShaderProfileInfo {
    VulkanShaderStage stage{VulkanShaderStage::Invalid};
    VkShaderModule shader{VK_NULL_HANDLE};
    std::vector<uint32_t> code;
    std::vector<uint64_t> profileData;
};

struct CollectedPipelineProfileInfo {
    VkPipelineBindPoint bindPoint{VK_PIPELINE_BIND_POINT_MAX_ENUM};
    VkPipeline pipeline{VK_NULL_HANDLE};
    std::vector<CollectedShaderProfileInfo> shaderInfos;
};

void SerializeToStream(const CollectedPipelineProfileInfo& info, WriteStream& stream);
void SerializeToStream(const CollectedShaderProfileInfo& info, WriteStream& stream);

void DeserializeFromStream(CollectedPipelineProfileInfo& info, const ReadStream& stream);
void DeserializeFromStream(CollectedShaderProfileInfo& info, const ReadStream& stream);

} // namespace OVS

#endif // LAYER__VULKAN_LAYER_H