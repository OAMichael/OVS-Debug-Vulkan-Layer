#ifndef UTILS__VULKAN_UTILS_H
#define UTILS__VULKAN_UTILS_H

#include <Vulkan.h>

namespace OVS {

static VkLayerInstanceCreateInfo* GetLayerInstanceCreateInfo(const void* pNext) {
    VkLayerInstanceCreateInfo* lici = (VkLayerInstanceCreateInfo*)pNext;

    while (lici && (lici->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO || lici->function != VK_LAYER_LINK_INFO)) {
        lici = (VkLayerInstanceCreateInfo*)lici->pNext;
    }

    return lici;
}

static VkLayerDeviceCreateInfo* GetLayerDeviceCreateInfo(const void* pNext) {
    VkLayerDeviceCreateInfo* ldci = (VkLayerDeviceCreateInfo*)pNext;

    while (ldci && (ldci->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO || ldci->function != VK_LAYER_LINK_INFO)) {
        ldci = (VkLayerDeviceCreateInfo*)ldci->pNext;
    }

    return ldci;
}

static void PatchDispatchKey(VkDevice device, VkCommandBuffer commandBuffer) {
    void* dispatchKey = *reinterpret_cast<void**>(device);
    *reinterpret_cast<void**>(commandBuffer) = dispatchKey;
}

} // namespace OVS

#endif // UTILS__VULKAN_UTILS_H
