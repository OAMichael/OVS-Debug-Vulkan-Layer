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

    static inline VulkanGlobalDispatchTable& GetGlobalDispatchTable() { return globalTableNative_; }

    template <typename T>
    static inline VulkanInstanceDispatchTable& GetInstanceDispatchTable(T obj) {
        DispatchKey dispatchKey = GetDispatchKey(obj);
        return instanceTablesNative_[dispatchKey];
    }

    template <typename T>
    static inline VulkanDeviceDispatchTable& GetDeviceDispatchTable(T obj) {
        DispatchKey dispatchKey = GetDispatchKey(obj);
        return deviceTablesNative_[dispatchKey];
    }

    VulkanLayerTerminator() : VulkanLayerTerminatorBase(VulkanLayerType::Terminator) {}
    virtual ~VulkanLayerTerminator() {}
};

} // namespace OVS

#endif // VULKAN_LAYER_TERMINATOR_H