#ifndef VULKAN_LAYER_OVERLAY_H
#define VULKAN_LAYER_OVERLAY_H

#include <VulkanLayerPassThroughGenerated.h>
#include <VulkanDispatchTablesGenerated.h>

#include <nlohmann/json.hpp>

#include <vector>
#include <string>
#include <unordered_map>

#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_vulkan.h>

namespace OVS {

class VulkanLayerOverlay : public VulkanLayerPassThrough {
public:
    virtual VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) override;
    virtual void vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator) override;

    virtual VkResult vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices) override;

    virtual VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) override;
    virtual void vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) override;

    virtual VkResult vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) override;
    virtual void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) override;

    virtual VkResult vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) override;
    virtual void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) override;
    virtual VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) override;

    inline const VulkanLayerOverlaySettings& GetSettings() const { return settings_; }

    explicit VulkanLayerOverlay(const VulkanLayerOverlaySettings& settings);
    virtual ~VulkanLayerOverlay();

    static VulkanLayerOverlaySettings ParseSettingsFromJSON(const nlohmann::json& layerInfo);

private:
    static constexpr uint32_t OverlayInfoMaxDescriptorsCount = 1000;
    static constexpr uint32_t OverlayInfoMaxDescriptorSetsCount = 1000;

    enum SurfaceType {
        None = 0,
        Windows = 1,
        Android = 2,
    };

    struct OverlayInfo {
        VkDevice device{VK_NULL_HANDLE};
        VkQueue queue{VK_NULL_HANDLE};
        VkCommandPool cmdPool{VK_NULL_HANDLE};
        std::vector<VkCommandBuffer> cmdBufs;
        std::vector<VkSemaphore> semaphores;
        std::vector<VkFence> fences;

        VkRenderPass renderPass{VK_NULL_HANDLE};
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;

        VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    };

    struct InstanceInfo {
        uint32_t apiVersion{0};
    };

    struct PhysicalDeviceInfo {
        VkInstance instance{VK_NULL_HANDLE};
    };

    struct SurfaceInfo {
        SurfaceType type{SurfaceType::None};
        void* window{nullptr};
    };

    struct SwapchainInfo {
        VkDevice device{VK_NULL_HANDLE};
        VkSurfaceKHR surface{VK_NULL_HANDLE};
        uint32_t width{0};
        uint32_t height{0};
        VkFormat format{VK_FORMAT_UNDEFINED};
        VkColorSpaceKHR imageColorSpace{VK_COLOR_SPACE_MAX_ENUM_KHR};
        VkPresentModeKHR presentMode{VK_PRESENT_MODE_MAX_ENUM_KHR};
        std::vector<VkImage> images;
    };

    struct DeviceInfo {
        VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
        uint32_t graphicsQueueFamilyIndex{0};
        uint32_t computeQueueFamilyIndex{0};
        VkQueue graphicsQueue{VK_NULL_HANDLE};
        VkQueue computeQueue{VK_NULL_HANDLE};
    };

    struct ImGuiInfo {
        VkInstance instance{VK_NULL_HANDLE};
        VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
        VkDevice device{VK_NULL_HANDLE};
        uint32_t queueFamily{UINT32_MAX};
        VkQueue queue{VK_NULL_HANDLE};
        VkDescriptorPool descriptorPool{VK_NULL_HANDLE};

        ImGui_ImplVulkanH_Window windowData;
    };

    VkResult CreateOverlayInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const SwapchainInfo& swapchainInfo, OverlayInfo& overlayInfoOut);
    void DestroyOverlayInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const OverlayInfo& overlayInfo);

    VkResult CreateImGuiInfo(VkSwapchainKHR swapchain, const SwapchainInfo& swapchainInfo, const OverlayInfo& overlayInfo, ImGuiInfo& imguiInfoOut);
    void DestroyImGuiInfo(const ImGuiInfo& imguiInfo);

    void ReplaceWndProc(HWND hwnd);

    std::unordered_map<VkInstance, InstanceInfo> instanceInfos_;
    std::unordered_map<VkPhysicalDevice, PhysicalDeviceInfo> physicalDeviceInfos_;
    std::unordered_map<VkSurfaceKHR, SurfaceInfo> surfaceInfos_;
    std::unordered_map<VkSwapchainKHR, SwapchainInfo> swapchainInfos_;
    std::unordered_map<VkSwapchainKHR, OverlayInfo> overlayInfos_;
    std::unordered_map<VkDevice, DeviceInfo> deviceInfos_;

    std::unordered_map<VkSwapchainKHR, ImGuiInfo> imguiInfos_;

    VulkanLayerOverlaySettings settings_{};
};

} // namespace OVS

#endif // VULKAN_LAYER_OVERLAY_H