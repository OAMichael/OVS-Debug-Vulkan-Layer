#ifndef VULKAN_LAYER_SCREENSHOT_H
#define VULKAN_LAYER_SCREENSHOT_H

#include <VulkanLayerPassThroughGenerated.h>

#include <vector>
#include <string>
#include <unordered_map>

namespace OVS {

class VulkanLayerScreenshot : public VulkanLayerPassThrough {
public:
    virtual VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) override;
    virtual void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) override;

    virtual void vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue) override;
    virtual void vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue) override;

    virtual VkResult vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) override;
    virtual void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) override;
    virtual VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) override;

    uint32_t GetCurrentFrame() const { return currentFrame_; }

    explicit VulkanLayerScreenshot(const VulkanLayerScreenshotSettings& settings) : VulkanLayerPassThrough(VulkanLayerType::Screenshot), settings_{settings} {}
    virtual ~VulkanLayerScreenshot() {}

    static VulkanLayerScreenshotSettings ParseSettingsFromJSON(const nlohmann::json& layerInfo);

private:
    struct ScreenshotInfo {
        VkImage image{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkCommandPool cmdPool{VK_NULL_HANDLE};
        VkCommandBuffer cmdBuf{VK_NULL_HANDLE};
        VkFence fence{VK_NULL_HANDLE};

        VkDeviceSize offset{0};
        VkDeviceSize stride{0};

        inline bool IsValid() const {
            return image && memory && cmdPool && cmdBuf;
        }
    };

    struct SwapchainInfo {
        uint32_t width{0};
        uint32_t height{0};
        std::vector<VkImage> images;
        ScreenshotInfo screenshotInfo{};
    };

    struct QueueInfo {
        uint32_t queueFamilyIndex{0};
        uint32_t queueIndex{0};
    };

    struct DeviceInfo {
        VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
        std::unordered_map<VkQueue, QueueInfo> queueInfos;
    };

    VkResult InitScreenshotInfo(ScreenshotInfo& screenshotInfo, VkDevice device, VkQueue queue, uint32_t width, uint32_t height);
    void CleanupScreenshotInfo(ScreenshotInfo& screenshotInfo, VkDevice device);

    bool ShouldMakeScreenshot(uint32_t frame) const;

    inline VkResult vkQueuePresentKHRInternal(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
        VkResult res = next_->vkQueuePresentKHR(queue, pPresentInfo);
        if (res == VK_SUCCESS) {
            ++currentFrame_;
        }
        return res;
    }

    std::unordered_map<VkSwapchainKHR, SwapchainInfo> swapchainInfos_;
    std::unordered_map<VkDevice, DeviceInfo> deviceInfos_;
    std::unordered_map<VkQueue, VkDevice> queueToDevice_;

    VulkanLayerScreenshotSettings settings_{};

    uint32_t currentFrame_{1};
};

} // namespace OVS

#endif // VULKAN_LAYER_SCREENSHOT_H