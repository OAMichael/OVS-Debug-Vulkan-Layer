#ifndef LAYER__VULKAN_LAYER_H
#define LAYER__VULKAN_LAYER_H

#include <VulkanShader.h>
#include <CommonUtils.h>

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

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
    Overlay,
    APIDump,
    APITrace,
    GPUProfiler,
    ShaderProfiler,
    ShaderOptimizer,

    Count
};

static inline const char* GetLayerTypeName(VulkanLayerType type) {
    switch (type) {
        case VulkanLayerType::PassThrough:      return "PassThrough";
        case VulkanLayerType::TerminatorBase:   return "TerminatorBase";
        case VulkanLayerType::Terminator:       return "Terminator";
        case VulkanLayerType::Printer:          return "Printer";
        case VulkanLayerType::Screenshot:       return "Screenshot";
        case VulkanLayerType::Overlay:          return "Overlay";
        case VulkanLayerType::APIDump:          return "APIDump";
        case VulkanLayerType::APITrace:         return "APITrace";
        case VulkanLayerType::GPUProfiler:      return "GPUProfiler";
        case VulkanLayerType::ShaderProfiler:   return "ShaderProfiler";
        case VulkanLayerType::ShaderOptimizer:  return "ShaderOptimizer";
        default:                                return "Unknown";
    }
}

static inline const char* GetLayerReadableTypeName(VulkanLayerType type) {
    switch (type) {
    case VulkanLayerType::PassThrough:      return "Pass Through";
    case VulkanLayerType::TerminatorBase:   return "Terminator Base";
    case VulkanLayerType::Terminator:       return "Terminator";
    case VulkanLayerType::Printer:          return "Printer";
    case VulkanLayerType::Screenshot:       return "Screenshot";
    case VulkanLayerType::Overlay:          return "Overlay";
    case VulkanLayerType::APIDump:          return "API Dump";
    case VulkanLayerType::APITrace:         return "API Trace";
    case VulkanLayerType::GPUProfiler:      return "GPU Profiler";
    case VulkanLayerType::ShaderProfiler:   return "Shader Profiler";
    case VulkanLayerType::ShaderOptimizer:  return "Shader Optimizer";
    default:                                return "Unknown";
    }
}

static inline VulkanLayerType GetLayerTypeByName(std::string_view name) {
    static const std::unordered_map<std::string_view, VulkanLayerType> sNameToTypeMap = {
        {"PassThrough",         VulkanLayerType::PassThrough},
        {"TerminatorBase",      VulkanLayerType::TerminatorBase},
        {"Terminator",          VulkanLayerType::Terminator},
        {"Printer",             VulkanLayerType::Printer},
        {"Screenshot",          VulkanLayerType::Screenshot},
        {"Overlay",             VulkanLayerType::Overlay},
        {"APIDump",             VulkanLayerType::APIDump},
        {"APITrace",            VulkanLayerType::APITrace},
        {"GPUProfiler",         VulkanLayerType::GPUProfiler},
        {"ShaderProfiler",      VulkanLayerType::ShaderProfiler},
        {"ShaderOptimizer",     VulkanLayerType::ShaderOptimizer},
    };

    auto it = sNameToTypeMap.find(name);
    if (it != sNameToTypeMap.end()) {
        return it->second;
    }
    return VulkanLayerType::None;
}

enum class ShaderOptimizerMode {
    None = 0,
    Performance,
    Size,
    Custom
};

static inline const char* GetShaderOptimizerModeName(ShaderOptimizerMode mode) {
    switch (mode) {
        case ShaderOptimizerMode::Performance:  return "Performance";
        case ShaderOptimizerMode::Size:         return "Size";
        case ShaderOptimizerMode::Custom:       return "Custom";
        default:                                return "None";
    }
}

enum class ShaderOptimizerPass {
    None = 0,
    StripDebugInfo,                         // Strip debug info
    StripNonSemanticInfo,                   // Strip non-semantic info
    EliminateDeadFunctions,                 // Eliminate dead functions
    EliminateDeadMembers,                   // Eliminate dead structure members
    FoldSpecConstantOpAndComposite,         // Fold specialization constants
    UnifyConstant,                          // Merge constants with the same values
    EliminateDeadConstant,                  // Eliminate dead constants
    StrengthReduction,                      // Peepholes
    BlockMerge,                             // Merge basic block with single predecessor with its parent
    InlineExhaustive,                       // Inline everything
    InlineOpaque,                           // Inline functions with opaque type parameters or return type
    DeadBranchElim,                         // Eliminate dead branches
    AggressiveDCE,                          // Aggressive DCE
    RemoveUnusedInterfaceVariables,         // Remove unused interface variables
    RemoveDuplicates,                       // Remove duplicate instructions (capabilities, ExtInstImport, types, decorations)
    CFGCleanup,                             // Remove unreachable basic blocks
    DeadVariableElimination,                // Eliminate dead variables
    MergeReturn,                            // Merge function return basic blocks
    LocalRedundancyElimination,             // Local value numbering
    LoopInvariantCodeMotion,                // Move loop invariant instructions outside a loop
    LoopFission,                            // Loop fission
    LoopFusion,                             // Loop fusion
    LoopPeeling,                            // Loop peeling
    LoopUnswitch,                           // Move branches out of a loop
    RedundancyElimination,                  // Global value numbering
    CoditionalConstantPropagation,          // Conditional constant propagation
    IfConversion,                           // If conversion
    Simplification,                         // Instruction simplification
    LoopUnroll,                             // Loop unroll
    ConvertRelaxedToHalf,                   // Replace all float instructions with half instructions
    RelaxFloatOps,                          // Decorate all floats with RelaxedPrecision
    CopyPropagateArrays,                    // Propogate arrays
    VectorDCE,                              // DCE for vector components
    ReduceLoadSize,                         // Reduce loads
    CombineAccessChains,                    // Combine (propogate) access
    GraphicsRobustAccess,                   // Enable robust access
    InterfaceVariableScalarReplacement,     // Replace interface variables with scalars/vectors (mat4 -> vec4, vec4, vec4, vec4)
    TrimCapabilities,                       // Remove unused capabilities
    SplitCombinedImageSampler,              // Split combined images sampler into sampled image and sampler
};

static inline ShaderOptimizerPass GetShaderOptimizerPassByName(std::string_view name) {
    static const std::unordered_map<std::string_view, ShaderOptimizerPass> sNameToTypeMap = {
        {"StripDebugInfo",                      ShaderOptimizerPass::StripDebugInfo},
        {"StripNonSemanticInfo",                ShaderOptimizerPass::StripNonSemanticInfo},
        {"EliminateDeadFunctions",              ShaderOptimizerPass::EliminateDeadFunctions},
        {"EliminateDeadMembers",                ShaderOptimizerPass::EliminateDeadMembers},
        {"FoldSpecConstantOpAndComposite",      ShaderOptimizerPass::FoldSpecConstantOpAndComposite},
        {"UnifyConstant",                       ShaderOptimizerPass::UnifyConstant},
        {"EliminateDeadConstant",               ShaderOptimizerPass::EliminateDeadConstant},
        {"StrengthReduction",                   ShaderOptimizerPass::StrengthReduction},
        {"BlockMerge",                          ShaderOptimizerPass::BlockMerge},
        {"InlineExhaustive",                    ShaderOptimizerPass::InlineExhaustive},
        {"InlineOpaque",                        ShaderOptimizerPass::InlineOpaque},
        {"DeadBranchElim",                      ShaderOptimizerPass::DeadBranchElim},
        {"AggressiveDCE",                       ShaderOptimizerPass::AggressiveDCE},
        {"RemoveUnusedInterfaceVariables",      ShaderOptimizerPass::RemoveUnusedInterfaceVariables},
        {"RemoveDuplicates",                    ShaderOptimizerPass::RemoveDuplicates},
        {"CFGCleanup",                          ShaderOptimizerPass::CFGCleanup},
        {"DeadVariableElimination",             ShaderOptimizerPass::DeadVariableElimination},
        {"MergeReturn",                         ShaderOptimizerPass::MergeReturn},
        {"LocalRedundancyElimination",          ShaderOptimizerPass::LocalRedundancyElimination},
        {"LoopInvariantCodeMotion",             ShaderOptimizerPass::LoopInvariantCodeMotion},
        {"LoopFission",                         ShaderOptimizerPass::LoopFission},
        {"LoopFusion",                          ShaderOptimizerPass::LoopFusion},
        {"LoopPeeling",                         ShaderOptimizerPass::LoopPeeling},
        {"LoopUnswitch",                        ShaderOptimizerPass::LoopUnswitch},
        {"RedundancyElimination",               ShaderOptimizerPass::RedundancyElimination},
        {"CoditionalConstantPropagation",       ShaderOptimizerPass::CoditionalConstantPropagation},
        {"IfConversion",                        ShaderOptimizerPass::IfConversion},
        {"Simplification",                      ShaderOptimizerPass::Simplification},
        {"LoopUnroll",                          ShaderOptimizerPass::LoopUnroll},
        {"ConvertRelaxedToHalf",                ShaderOptimizerPass::ConvertRelaxedToHalf},
        {"RelaxFloatOps",                       ShaderOptimizerPass::RelaxFloatOps},
        {"CopyPropagateArrays",                 ShaderOptimizerPass::CopyPropagateArrays},
        {"VectorDCE",                           ShaderOptimizerPass::VectorDCE},
        {"ReduceLoadSize",                      ShaderOptimizerPass::ReduceLoadSize},
        {"CombineAccessChains",                 ShaderOptimizerPass::CombineAccessChains},
        {"GraphicsRobustAccess",                ShaderOptimizerPass::GraphicsRobustAccess},
        {"InterfaceVariableScalarReplacement",  ShaderOptimizerPass::InterfaceVariableScalarReplacement},
        {"TrimCapabilities",                    ShaderOptimizerPass::TrimCapabilities},
        {"SplitCombinedImageSampler",           ShaderOptimizerPass::SplitCombinedImageSampler},
    };

    auto it = sNameToTypeMap.find(name);
    if (it != sNameToTypeMap.end()) {
        return it->second;
    }
    return ShaderOptimizerPass::None;
}

class VulkanLayerInterface;
using VulkanLayerPtr = std::unique_ptr<VulkanLayerInterface>;

constexpr const char PrinterDefaultFilename[] = "stdout";
constexpr const char ScreenshotDefaultFileBaseName[] = "screenshot";
constexpr const char APIDumpDefaultFilename[] = "stdout";
constexpr const char APITraceDefaultFilename[] = "apitrace.ovs";
constexpr const char GPUProfilerDefaultFilename[] = "gpuprof.ovs";
constexpr size_t APITraceDefaultFlushSize = 200 * 1024 * 1024;
constexpr const char ShaderProfilerDefaultFilename[] = "shaderprof.ovs";

struct VulkanLayerPrinterSettings {
    std::string filename{PrinterDefaultFilename};
};

struct VulkanLayerScreenshotSettings {
    std::string fileBaseName{ScreenshotDefaultFileBaseName};
    std::vector<FrameRange> frameRanges;
};

struct VulkanLayerOverlaySettings {
    bool multipleViewports{false};
};

struct VulkanLayerAPIDumpSettings {
    std::string filename{APIDumpDefaultFilename};
};

struct VulkanLayerAPITraceSettings {
    std::string filename{APITraceDefaultFilename};
    size_t flushSize{APITraceDefaultFlushSize};
};

struct VulkanLayerGPUProfilerSettings {
    std::string filename{GPUProfilerDefaultFilename};
    bool useZoneBarriers{false};
};

struct VulkanLayerShaderProfilerSettings {
    std::string filename{ShaderProfilerDefaultFilename};
};

struct VulkanLayerShaderOptimizerSettings {
    ShaderOptimizerMode mode{ShaderOptimizerMode::None};
    std::vector<ShaderOptimizerPass> customPasses;
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

struct GPUProfilerFileHeader {
    uint64_t byteSize{0};
    float timestampPeriod{0};
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

struct GPUZone {
    std::string name;
    uint64_t begin{0};
    uint64_t end{0};
    std::vector<GPUZone> children;
};

struct GPUProfileCommandBufferInfo {
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    GPUZone rootZone;
};

struct GPUProfileFrameInfo {
    uint32_t frame{0};
    std::vector<GPUProfileCommandBufferInfo> commandBufferInfos;
};

struct GPUProfileInfo {
    std::vector<GPUProfileFrameInfo> frameInfos;
};

void SerializeToStream(const CollectedPipelineProfileInfo& info, WriteStream& stream);
void SerializeToStream(const CollectedShaderProfileInfo& info, WriteStream& stream);
void SerializeToStream(const GPUProfileInfo& info, WriteStream& stream);
void SerializeToStream(const GPUProfileFrameInfo& info, WriteStream& stream);
void SerializeToStream(const GPUProfileCommandBufferInfo& info, WriteStream& stream);
void SerializeToStream(const GPUZone& info, WriteStream& stream);

void DeserializeFromStream(CollectedPipelineProfileInfo& info, const ReadStream& stream);
void DeserializeFromStream(CollectedShaderProfileInfo& info, const ReadStream& stream);
void DeserializeFromStream(GPUProfileInfo& info, const ReadStream& stream);
void DeserializeFromStream(GPUProfileFrameInfo& info, const ReadStream& stream);
void DeserializeFromStream(GPUProfileCommandBufferInfo& info, const ReadStream& stream);
void DeserializeFromStream(GPUZone& info, const ReadStream& stream);

} // namespace OVS

#endif // LAYER__VULKAN_LAYER_H
