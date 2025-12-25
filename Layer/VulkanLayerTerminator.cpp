#include <VulkanUtils.h>
#include <VulkanLayerTerminator.h>

#include <string>
#include <unordered_map>

namespace OVS {

extern std::unordered_map<std::string, PFN_vkVoidFunction> sFunctionTable;

VkResult VulkanLayerTerminator::vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
    VkLayerInstanceCreateInfo* layerCreateInfo = GetLayerInstanceCreateInfo(pCreateInfo);
    if (!layerCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateInstance pfnCreateInstance = (PFN_vkCreateInstance)gipa(VK_NULL_HANDLE, "vkCreateInstance");
    if (!pfnCreateInstance) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult ret = pfnCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (ret != VK_SUCCESS) {
        return ret;
    }

    LoadInstanceDispatchTable(gipa, *pInstance, dispatchTableNative_);
    return VK_SUCCESS;
}

VkResult VulkanLayerTerminator::vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    VkLayerDeviceCreateInfo* layerCreateInfo = GetLayerDeviceCreateInfo(pCreateInfo);
    if (!layerCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetDeviceProcAddr gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateDevice pfnCreateDevice = dispatchTableNative_.vkCreateDevice;
    if (!pfnCreateDevice) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult ret = pfnCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (ret != VK_SUCCESS) {
        return ret;
    }

    LoadDeviceDispatchTable(gdpa, *pDevice, dispatchTableNative_);
    return VK_SUCCESS;
}

PFN_vkVoidFunction VulkanLayerTerminator::vkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    auto it = sFunctionTable.find(pName);
    if (it != sFunctionTable.end()) {
        return it->second;
    }
    return dispatchTableNative_.vkGetInstanceProcAddr(instance, pName);
}

PFN_vkVoidFunction VulkanLayerTerminator::vkGetDeviceProcAddr(VkDevice device, const char* pName)
{
    auto it = sFunctionTable.find(pName);
    if (it != sFunctionTable.end()) {
        return it->second;
    }
    return dispatchTableNative_.vkGetDeviceProcAddr(device, pName);
}

} // namespace OVS