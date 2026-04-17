#ifndef VULKAN_LAYER_SHADER_OPTIMIZER_H
#define VULKAN_LAYER_SHADER_OPTIMIZER_H

#include <VulkanLayerPassThroughGenerated.h>

#include <nlohmann/json.hpp>

namespace spvtools {
class Optimizer;
}

namespace OVS {

class VulkanLayerShaderOptimizer : public VulkanLayerPassThrough {
public:
    virtual VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) override;

    explicit VulkanLayerShaderOptimizer(const VulkanLayerShaderOptimizerSettings& settings) : VulkanLayerPassThrough(VulkanLayerType::ShaderOptimizer), settings_{settings} {}
    virtual ~VulkanLayerShaderOptimizer() {}

    static VulkanLayerShaderOptimizerSettings ParseSettingsFromJSON(const nlohmann::json& layerInfo);

private:
    void SetupOptimizer(spvtools::Optimizer& opt);

    VulkanLayerShaderOptimizerSettings settings_{};
};

} // namespace OVS

#endif // VULKAN_LAYER_SHADER_OPTIMIZER_H