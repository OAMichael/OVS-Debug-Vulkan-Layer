#include <VulkanUtils.h>
#include <VulkanLayerTerminator.h>

#include <string>
#include <unordered_map>

namespace OVS {

extern std::unordered_map<std::string, PFN_vkVoidFunction> sFunctionTable;

std::unordered_map<DispatchKey, VulkanInstanceDispatchTable> VulkanLayerTerminatorBase::instanceTablesNative_;
std::unordered_map<DispatchKey, VulkanDeviceDispatchTable>   VulkanLayerTerminatorBase::deviceTablesNative_;
VulkanGlobalDispatchTable                                    VulkanLayerTerminatorBase::globalTableNative_;

VkResult VulkanLayerTerminator::vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
    VkLayerInstanceCreateInfo* layerCreateInfo = GetLayerInstanceCreateInfo(pCreateInfo);
    if (!layerCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateInstance pfnCreateInstance = (PFN_vkCreateInstance)gipa(nullptr, "vkCreateInstance");
    if (!pfnCreateInstance) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult ret = pfnCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (ret != VK_SUCCESS) {
        return ret;
    }

    VkInstance instance = *pInstance;

    DispatchKey dispatchKey = GetDispatchKey(instance);
    auto& instanceDispatchTable = instanceTablesNative_[dispatchKey];
    LoadInstanceDispatchTable(gipa, instance, instanceDispatchTable);

    auto& globalDispatchTable = globalTableNative_;
    LoadGlobalDispatchTable(gipa, globalDispatchTable);

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

    DispatchKey dispatchKey = GetDispatchKey(physicalDevice);
    const auto& dispatchTable = instanceTablesNative_[dispatchKey];
    PFN_vkCreateDevice pfnCreateDevice = dispatchTable.vkCreateDevice;
    if (!pfnCreateDevice) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult ret = pfnCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (ret != VK_SUCCESS) {
        return ret;
    }

    VkDevice device = *pDevice;

    dispatchKey = GetDispatchKey(device);
    auto& deviceDispatchTable = deviceTablesNative_[dispatchKey];
    LoadDeviceDispatchTable(gdpa, *pDevice, deviceDispatchTable);

    return VK_SUCCESS;
}

PFN_vkVoidFunction VulkanLayerTerminator::vkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    auto it = sFunctionTable.find(pName);
    if (it != sFunctionTable.end()) {
        return it->second;
    }

    DispatchKey dispatchKey = GetDispatchKey(instance);
    const auto& dispatchTable = instanceTablesNative_[dispatchKey];
    return dispatchTable.vkGetInstanceProcAddr(instance, pName);
}

PFN_vkVoidFunction VulkanLayerTerminator::vkGetDeviceProcAddr(VkDevice device, const char* pName)
{
    auto it = sFunctionTable.find(pName);
    if (it != sFunctionTable.end()) {
        return it->second;
    }

    DispatchKey dispatchKey = GetDispatchKey(device);
    const auto& dispatchTable = deviceTablesNative_[dispatchKey];
    return dispatchTable.vkGetDeviceProcAddr(device, pName);
}

} // namespace OVS