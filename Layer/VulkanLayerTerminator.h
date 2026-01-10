#ifndef VULKAN_LAYER_TERMINATOR_H
#define VULKAN_LAYER_TERMINATOR_H

#include <VulkanLayerTerminatorBaseGenerated.h>

namespace OVS {

class VulkanLayerTerminator : public VulkanLayerTerminatorBase {
public:
    virtual VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) override;
    virtual VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) override;
    virtual PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName) override;
    virtual PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice device, const char* pName) override;

    VulkanLayerTerminator() : VulkanLayerTerminatorBase(VulkanLayerType::Terminator) {}
    virtual ~VulkanLayerTerminator() {}
};

} // namespace OVS

#endif // VULKAN_LAYER_TERMINATOR_H