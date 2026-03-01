#ifndef VULKAN_LAYER_OVERLAY_H
#define VULKAN_LAYER_OVERLAY_H

#include <VulkanLayerPassThroughGenerated.h>

#include <vector>
#include <string>
#include <unordered_map>

namespace OVS {

class VulkanLayerOverlay : public VulkanLayerPassThrough {
public:
    virtual VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) override;
    virtual void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) override;

    virtual VkResult vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) override;
    virtual void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) override;
    virtual VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) override;

    explicit VulkanLayerOverlay(const VulkanLayerOverlaySettings& settings) : VulkanLayerPassThrough(VulkanLayerType::Overlay), settings_{settings} {}
    virtual ~VulkanLayerOverlay() {}

private:
    struct OverlayInfo {
        VkDevice device{VK_NULL_HANDLE};
        VkQueue queue{VK_NULL_HANDLE};
        VkCommandPool cmdPool{VK_NULL_HANDLE};
        std::vector<VkCommandBuffer> cmdBufs;
        std::vector<VkSemaphore> semaphores;
        std::vector<VkFence> fences;
        std::vector<VkRenderPass> renderPasses;
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;
    };

    struct SwapchainInfo {
        uint32_t width{0};
        uint32_t height{0};
        VkFormat format{VK_FORMAT_UNDEFINED};
        std::vector<VkImage> images;
    };

    struct DeviceInfo {
        VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
        uint32_t graphicsQueueFamilyIndex{0};
        uint32_t computeQueueFamilyIndex{0};
        VkQueue graphicsQueue{VK_NULL_HANDLE};
        VkQueue computeQueue{VK_NULL_HANDLE};
    };

    VkResult CreateOverlayInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const SwapchainInfo& swapchainInfo, OverlayInfo& overlayInfoOut);
    void DestroyOverlayInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const OverlayInfo& overlayInfo);

    std::unordered_map<VkSwapchainKHR, SwapchainInfo> swapchainInfos_;
    std::unordered_map<VkSwapchainKHR, OverlayInfo> overlayInfos_;
    std::unordered_map<VkDevice, DeviceInfo> deviceInfos_;

    VulkanLayerOverlaySettings settings_{};
};

} // namespace OVS

#endif // VULKAN_LAYER_OVERLAY_H