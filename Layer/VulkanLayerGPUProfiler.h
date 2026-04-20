#ifndef VULKAN_LAYER_GPU_PROFILER_H
#define VULKAN_LAYER_GPU_PROFILER_H

#include <VulkanLayerPassThroughGenerated.h>

#include <nlohmann/json.hpp>

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <mutex>

namespace OVS {

class VulkanLayerGPUProfiler : public VulkanLayerPassThrough {
public:
    virtual VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) override;

    virtual VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) override;
    virtual void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) override;

    virtual void vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) override;
    virtual void vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) override;

    virtual VkResult vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) override;
    virtual void vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator) override;
    virtual VkResult vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) override;

    virtual VkResult vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers) override;
    virtual void vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) override;

    virtual VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) override;
    virtual VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer) override;
    virtual VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) override;

    virtual void vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) override;
    virtual void vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer) override;

    virtual VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) override;
    virtual VkResult vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) override;
    virtual VkResult vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) override;

    virtual VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) override;

    explicit VulkanLayerGPUProfiler(const VulkanLayerGPUProfilerSettings& settings);
    virtual ~VulkanLayerGPUProfiler();

    static VulkanLayerGPUProfilerSettings ParseSettingsFromJSON(const nlohmann::json& layerInfo);

private:
    static constexpr uint32_t MaxQueryPerCommandBuffer = 2048;

    struct QueueInfo {
        uint32_t queueFamilyIndex{0};
        uint32_t queueIndex{0};
        uint32_t timestampValidBits{0};
    };

    struct CommandPoolInfo {
        std::unordered_set<VkCommandBuffer> commandBuffers;
    };

    enum CommandBufferState {
        Initial,
        Recording,
        Executable,
        Pending
    };

    struct GPUZoneInfo {
        std::string name;
        uint32_t queryBegin{0};
        uint32_t queryEnd{0};
        std::vector<GPUZoneInfo> children;
    };

    struct CommandBufferInfo {
        CommandBufferState state{CommandBufferState::Initial};
        VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
        VkQueryPool queryPool{VK_NULL_HANDLE};
        uint32_t queryCount{0};
        uint32_t submitFrame{0};
        GPUZoneInfo rootZone;
        std::stack<GPUZoneInfo*> zonesStack;
    };

    class DeviceInfo {
    public:
        explicit DeviceInfo(VulkanLayerInterface& layer) : layer_(layer) {}

        VkDevice device{VK_NULL_HANDLE};
        VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
        std::vector<VkQueueFamilyProperties> queueFamilyProperties;
        std::unordered_map<VkQueue, QueueInfo> queueInfos;
        std::unordered_map<VkCommandPool, CommandPoolInfo> commandPoolInfos;
        std::unordered_map<VkCommandBuffer, CommandBufferInfo> commandBufferInfos;

        inline VkQueryPool AllocateQueryPool() {
            VkQueryPool queryPool = VK_NULL_HANDLE;
            if (!freeQueryPools_.empty()) {
                auto b = freeQueryPools_.begin();
                queryPool = *b;
                freeQueryPools_.erase(b);
            }
            else {
                VkQueryPoolCreateInfo qpci{};
                qpci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
                qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
                qpci.queryCount = MaxQueryPerCommandBuffer;

                VkResult res = layer_.vkCreateQueryPool(device, &qpci, nullptr, &queryPool);
                if (res == VK_SUCCESS) {
                    allQueryPools_.insert(queryPool);
                }
            }
            return queryPool;
        }

        inline void FreeQueryPool(VkQueryPool queryPool) {
            freeQueryPools_.insert(queryPool);
        }

        inline void DestroyQueryPools() {
            for (const auto& queryPool : allQueryPools_) {
                layer_.vkDestroyQueryPool(device, queryPool, nullptr);
            }
            allQueryPools_.clear();
            freeQueryPools_.clear();
        }

    private:
        VulkanLayerInterface& layer_;

        std::unordered_set<VkQueryPool> allQueryPools_;
        std::unordered_set<VkQueryPool> freeQueryPools_;
    };

    inline CommandBufferInfo* GetCommandBufferInfo(VkCommandBuffer commandBuffer) {
        auto deviceIt = commandBufferToDeviceMap_.find(commandBuffer);
        if (deviceIt == commandBufferToDeviceMap_.end()) {
            return nullptr;
        }

        VkDevice device = deviceIt->second;

        auto deviceInfoIt = deviceInfoMap_.find(device);
        if (deviceInfoIt == deviceInfoMap_.end()) {
            return nullptr;
        }

        auto& deviceInfo = deviceInfoIt->second;
        auto& commandBufferInfos = deviceInfo.commandBufferInfos;

        auto commandBufferInfoIt = commandBufferInfos.find(commandBuffer);
        if (commandBufferInfoIt == commandBufferInfos.end()) {
            return nullptr;
        }

        return &(commandBufferInfoIt->second);
    }

    inline VkDevice GetDeviceByQueue(VkQueue queue) const {
        auto it = queueToDeviceMap_.find(queue);
        if (it == queueToDeviceMap_.end()) {
            return VK_NULL_HANDLE;
        }
        return it->second;
    }

    static inline constexpr VkTimeDomainKHR GetRequiredTimeDomain() { return VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR; }

    bool CalibrateTimestamps(VkDevice device);
    bool CollectCommandBufferInfo(const CommandBufferInfo& commandBufferInfo);
    void ConvertGPUZones(const std::vector<uint64_t>& queryResults, const GPUZoneInfo& rawZone, GPUZone& profileZone);

    void SetupOriginTimestamp();
    void StripProfileInfo();
    bool SaveProfileInfo() const;

    VulkanLayerGPUProfilerSettings settings_{};

    std::unordered_map<VkDevice, DeviceInfo> deviceInfoMap_;
    std::unordered_map<VkQueue, VkDevice> queueToDeviceMap_;
    std::unordered_map<VkCommandBuffer, VkDevice> commandBufferToDeviceMap_;

    std::mutex mutex_;

    uint32_t currentFrame_{1};

    bool hasCalibratedTimestamps_{false};
    uint64_t originTimestamp_{InvalidTimestamp};
    float timestampPeriod_{0.0f};

    uint64_t calibratedTimestamp_{0};
    uint64_t calibratedPerfCounter_{0};
    uint64_t perfFrequency_{0};

    GPUProfileInfo profileInfo_;
};

} // namespace OVS

#endif // VULKAN_LAYER_GPU_PROFILER_H