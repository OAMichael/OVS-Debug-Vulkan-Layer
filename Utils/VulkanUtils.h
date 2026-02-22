#ifndef UTILS__VULKAN_UTILS_H
#define UTILS__VULKAN_UTILS_H

#include <Vulkan.h>

#include <optional>

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

static void PatchDispatchKey(VkDevice device, VkQueue queue) {
    void* dispatchKey = *reinterpret_cast<void**>(device);
    *reinterpret_cast<void**>(queue) = dispatchKey;
}

static std::optional<uint32_t> GetMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties& properties, uint32_t typeBits, VkMemoryPropertyFlags propertyFlags) {
    for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
        if ((typeBits & 1) == 1) {
            if ((properties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
                return i;
            }
        }
        typeBits >>= 1;
    }
    return std::nullopt;
}

static const void* GetStructFromPNextChain(VkStructureType sType, const void* pNext) {
    while (pNext) {
        const VkBaseInStructure* pBaseIn = reinterpret_cast<const VkBaseInStructure*>(pNext);
        if (pBaseIn->sType == sType) {
            break;
        }
        pNext = pBaseIn->pNext;
    }
    return pNext;
}

} // namespace OVS

#endif // UTILS__VULKAN_UTILS_H
