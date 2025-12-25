#ifndef VULKAN_LAYER_MANAGER_H
#define VULKAN_LAYER_MANAGER_H

#include <VulkanLayerInterfaceGenerated.h>

#include <memory>
#include <vector>

namespace OVS {

using VulkanLayerPtr = std::unique_ptr<VulkanLayerInterface>;

class VulkanLayerManager {
public:
    void Init();

    inline VulkanLayerPtr& GetFrontLayer() { return layers_.front(); }

private:
    bool inited_{false};

    std::vector<VulkanLayerPtr> layers_;
};

} // namespace OVS

#endif // VULKAN_LAYER_MANAGER_H