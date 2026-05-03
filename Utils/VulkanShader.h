#ifndef UTILS__VULKAN_SHADER_H
#define UTILS__VULKAN_SHADER_H

#include <Vulkan.h>

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

namespace spvtools::opt {
class Module;
}

namespace OVS {

enum class VulkanShaderStage {
    // Graphics
    Vertex = 0,
    TessCtrl = 1,
    TessEval = 2,
    Geometry = 3,
    Fragment = 4,

    // Compute
    Compute = 5,

    // Ray Tracing
    RayGeneration = 6,
    Intersection = 7,
    AnyHit = 8,
    ClosestHit = 9,
    Miss = 10,
    Callable = 11,

    Count,

    Invalid = Count,
};

constexpr static const char* VulkanShaderStageNames[(size_t)VulkanShaderStage::Count] = {
    "Vertex",
    "TessCtrl",
    "TessEval",
    "Geometry",
    "Fragment",
    "Compute",
    "RayGeneration",
    "Intersection",
    "AnyHit",
    "ClosestHit",
    "Miss",
    "Callable"
};

constexpr static inline const char* GetVulkanShaderStageName(VulkanShaderStage stage) {
    return VulkanShaderStageNames[(size_t)stage];
}

constexpr static inline VkShaderStageFlagBits GetVulkanShaderStageBit(VulkanShaderStage stage) {
    switch (stage) {
        case VulkanShaderStage::Vertex:        return VK_SHADER_STAGE_VERTEX_BIT;
        case VulkanShaderStage::TessCtrl:      return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case VulkanShaderStage::TessEval:      return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case VulkanShaderStage::Geometry:      return VK_SHADER_STAGE_GEOMETRY_BIT;
        case VulkanShaderStage::Fragment:      return VK_SHADER_STAGE_FRAGMENT_BIT;
        case VulkanShaderStage::Compute:       return VK_SHADER_STAGE_COMPUTE_BIT;
        case VulkanShaderStage::RayGeneration: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case VulkanShaderStage::Intersection:  return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case VulkanShaderStage::AnyHit:        return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case VulkanShaderStage::ClosestHit:    return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case VulkanShaderStage::Miss:          return VK_SHADER_STAGE_MISS_BIT_KHR;
        case VulkanShaderStage::Callable:      return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        default:                               return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM; 
    }
}

constexpr static inline const char* GetVulkanPipelineBindPointName(VkPipelineBindPoint bindPoint) {
    switch (bindPoint) {
        case VK_PIPELINE_BIND_POINT_GRAPHICS:        return "Graphics";
        case VK_PIPELINE_BIND_POINT_COMPUTE:         return "Compute";
        case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR: return "RayTracing";
        default:                                     return "Unknown";
    }
}

void PrintSPIRV(const uint32_t* pCode, uint32_t codeSize, std::ostream& out);
void PrintSPIRV(const std::vector<uint32_t>& code, std::ostream& out);
void PrintSPIRV(const spvtools::opt::Module& m, std::ostream& out);


struct SPIRVDebugInfo {
    std::string filename;
    std::string source;
    std::unordered_map<uint32_t, size_t> instLines;
};

struct SPIRVProfileInfo {
    SPIRVDebugInfo debugInfo{};

    uint64_t totalFuncExecuted{0};
    uint64_t totalBBExecuted{0};
    uint64_t totalInstExecuted{0};

    std::unordered_map<uint32_t, uint64_t> funcExecuted;
    std::unordered_map<uint32_t, uint64_t> bbExecuted;
    std::unordered_map<uint32_t, uint64_t> instExecuted;

    std::unordered_map<size_t, uint64_t> linesExecuted;
};

bool ParseSPIRVDebugInfo(const spvtools::opt::Module& m, SPIRVDebugInfo& spvDebugInfoOut);
bool SetupSPIRVProfileInfo(const spvtools::opt::Module& m, const std::vector<uint64_t>& profileData, SPIRVProfileInfo& spvProfileInfoOut);
void ComputeLinesExecuted(const spvtools::opt::Module& m, SPIRVProfileInfo& spvProfileInfo);

} // namespace OVS

#endif // UTILS__VULKAN_SHADER_H
