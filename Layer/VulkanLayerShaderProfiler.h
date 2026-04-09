#ifndef VULKAN_LAYER_SHADER_PROFILER_H
#define VULKAN_LAYER_SHADER_PROFILER_H

#include <VulkanLayerPassThroughGenerated.h>
#include <VulkanShader.h>

#include <unordered_map>

namespace OVS {

class VulkanLayerShaderProfiler : public VulkanLayerPassThrough {
public:
    virtual VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) override;
    virtual void vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) override;

    virtual VkResult vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) override;

    virtual VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) override;
    virtual void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) override;

    virtual VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) override;
    virtual void vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) override;

    virtual VkResult vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) override;
    virtual void vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) override;

    virtual VkResult vkCreateGraphicsPipelines(VkDevice device,
                                               VkPipelineCache pipelineCache,
                                               uint32_t createInfoCount,
                                               const VkGraphicsPipelineCreateInfo* pCreateInfos,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkPipeline* pPipelines) override;

    virtual VkResult vkCreateComputePipelines(VkDevice device,
                                              VkPipelineCache pipelineCache,
                                              uint32_t createInfoCount,
                                              const VkComputePipelineCreateInfo* pCreateInfos,
                                              const VkAllocationCallbacks* pAllocator,
                                              VkPipeline* pPipelines) override;

    virtual VkResult vkCreateRayTracingPipelinesKHR(VkDevice device,
                                                    VkDeferredOperationKHR deferredOperation,
                                                    VkPipelineCache pipelineCache,
                                                    uint32_t createInfoCount,
                                                    const VkRayTracingPipelineCreateInfoKHR* pCreateInfos,
                                                    const VkAllocationCallbacks* pAllocator,
                                                    VkPipeline* pPipelines) override;

    virtual void vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) override;

    virtual VkResult vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) override;
    virtual VkResult vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData) override;
    virtual VkDeviceSize vkGetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader) override;

    virtual void vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) override;

    explicit VulkanLayerShaderProfiler(const VulkanLayerShaderProfilerSettings& settings) : VulkanLayerPassThrough(VulkanLayerType::ShaderProfiler), settings_{settings} {}
    virtual ~VulkanLayerShaderProfiler() { SaveCollectedProfileInfo(); }

    static VulkanLayerShaderProfilerSettings ParseSettingsFromJSON(const nlohmann::json& layerInfo);

private:
    struct InstanceInfo {
        uint32_t apiVersion{0};
    };

    struct PhysicalDeviceInfo {
        VkInstance instance{VK_NULL_HANDLE};
        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    };

    struct DeviceInfo {
        VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
        uint32_t transferQueueFamilyIndex{0};
        VkQueue transferQueue{VK_NULL_HANDLE};
    };

    struct ShaderInfo {
        std::vector<uint32_t> code;
    };

    struct PipelineLayoutInfo {
        std::vector<VkDescriptorSetLayout> setLayouts;
        std::vector<VkPushConstantRange> pushConstantRanges;
    };

    struct ShaderProfileStorage {
        VkBuffer localBuffer{VK_NULL_HANDLE};
        VkBuffer stagingBuffer{VK_NULL_HANDLE};
        VkDeviceMemory localMemory{VK_NULL_HANDLE};
        VkDeviceMemory stagingMemory{VK_NULL_HANDLE};
    };

    struct ShaderProfileInfo {
        VulkanShaderStage stage{VulkanShaderStage::Invalid};
        VkShaderModule origShader{VK_NULL_HANDLE};
        VkShaderModule modifiedShader{VK_NULL_HANDLE};
        std::vector<uint32_t> origCode;
        std::vector<uint32_t> modifiedCode;
        uint32_t shaderBBCount{0};
        uint32_t profileSet{0};
        uint32_t profileBinding{0};
        ShaderProfileStorage storage;
    };

    struct PipelineProfileCommandInfo {
        VkQueue queue{VK_NULL_HANDLE};
        VkCommandPool cmdPool{VK_NULL_HANDLE};
        VkCommandBuffer cmdBuf{VK_NULL_HANDLE};
        VkFence fence{VK_NULL_HANDLE};
    };

    struct PipelineProfileInfo {
        VkDevice device{VK_NULL_HANDLE};
        VkPipelineBindPoint bindPoint{VK_PIPELINE_BIND_POINT_MAX_ENUM};
        VkPipeline origPipeline{VK_NULL_HANDLE};
        VkPipeline modifiedPipeline{VK_NULL_HANDLE};
        VkPipelineLayout origLayout{VK_NULL_HANDLE};
        VkPipelineLayout modifiedLayout{VK_NULL_HANDLE};
        VkDescriptorSetLayout profileSetLayout{VK_NULL_HANDLE};
        VkDescriptorPool profileDescriptorPool{VK_NULL_HANDLE};
        VkDescriptorSet profileDescriptorSet{VK_NULL_HANDLE};
        PipelineProfileCommandInfo commandInfo;
        std::vector<ShaderProfileInfo> shaderInfos;
    };

    bool CreateShaderProfileInfos(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<VkPipelineShaderStageCreateInfo>& stageInfos, uint32_t profileSet, std::vector<ShaderProfileInfo>& shaderInfosOut) const;
    void DestroyShaderProfileInfos(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<ShaderProfileInfo>& shaderInfos) const;

    bool CreateShaderProfileInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const VkPipelineShaderStageCreateInfo& stageInfo, uint32_t profileSet, uint32_t profileBinding, ShaderProfileInfo& shaderInfoOut) const;
    void DestroyShaderProfileInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const ShaderProfileInfo& shaderInfo) const;

    bool ModifySPIRV(const std::vector<uint32_t>& origCode, uint32_t profileSet, uint32_t profileBinding, std::vector<uint32_t>& modifiedCodeOut, uint32_t& bbCountOut, VulkanShaderStage& stageOut) const;

    bool CreateShaderProfileShader(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<uint32_t>& modifiedCode, VkShaderModule& shaderOut) const;
    void DestroyShaderProfileShader(VkDevice device, const VkAllocationCallbacks* pAllocator, VkShaderModule shader) const;

    bool CreateShaderProfileStorage(VkDevice device, const VkAllocationCallbacks* pAllocator, uint32_t bbCount, ShaderProfileStorage& storageOut) const;
    void DestroyShaderProfileStorage(VkDevice device, const VkAllocationCallbacks* pAllocator, const ShaderProfileStorage& storage) const;

    bool CreatePipelineProfileCommandInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, PipelineProfileCommandInfo& commandInfoOut) const;
    void DestroyPipelineProfileCommandInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const PipelineProfileCommandInfo& commandInfo) const;

    bool CreateProfileDescriptorSetLayout(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<ShaderProfileInfo>& shaderInfos, VkDescriptorSetLayout& setLayoutOut) const;
    void DestroyProfileDescriptorSetLayout(VkDevice device, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout setLayout) const;

    bool CreateProfileDescriptorPool(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<ShaderProfileInfo>& shaderInfos, VkDescriptorPool& descriptorPoolOut) const;
    void DestroyProfileDescriptorPool(VkDevice device, const VkAllocationCallbacks* pAllocator, VkDescriptorPool descriptorPool) const;

    bool SetupProfileDescriptorSet(VkDevice device, const std::vector<ShaderProfileInfo>& shaderInfos, VkDescriptorSetLayout setLayout, VkDescriptorPool descriptorPool, VkDescriptorSet& descriptorSetOut) const;

    bool CreateProfilePipelineLayout(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<ShaderProfileInfo>& shaderInfos,
        VkPipelineLayout origLayout, VkDescriptorSetLayout profileSetLayout, VkPipelineLayout& pipelineLayoutOut) const;
    void DestroyProfilePipelineLayout(VkDevice device, const VkAllocationCallbacks* pAllocator, VkPipelineLayout pipelineLayout) const;

    bool BindProfilePipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) const;

    bool CollectProfileData(VkDevice device, const PipelineProfileInfo& pipelineProfileInfo);
    bool SaveCollectedProfileInfo() const;

    VulkanLayerShaderProfilerSettings settings_{};

    std::unordered_map<VkInstance, InstanceInfo> instanceInfoMap_;
    std::unordered_map<VkPhysicalDevice, PhysicalDeviceInfo> physicalDeviceInfoMap_;
    std::unordered_map<VkDevice, DeviceInfo> deviceInfoMap_;
    std::unordered_map<VkShaderModule, ShaderInfo> shaderInfoMap_;
    std::unordered_map<VkPipelineLayout, PipelineLayoutInfo> pipelineLayoutInfoMap_;

    std::unordered_map<VkPipeline, PipelineProfileInfo> pipelineProfileInfoMap_;

    std::vector<CollectedPipelineProfileInfo> collectedProfileInfos_;
};

} // namespace OVS

#endif // VULKAN_LAYER_SHADER_PROFILER_H