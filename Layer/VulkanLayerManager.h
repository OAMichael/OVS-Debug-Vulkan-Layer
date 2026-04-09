#ifndef VULKAN_LAYER_MANAGER_H
#define VULKAN_LAYER_MANAGER_H

#include <VulkanLayerInterfaceGenerated.h>

#include <vector>

namespace OVS {

class VulkanLayerManager {
public:
    void Init();
    void Cleanup();

    inline VulkanLayerPtr& GetFrontLayer() { return layers_.front(); }

private:
    bool CreateLayersFromJSON(const std::string& settingsPath);
    bool ContainsLayer(VulkanLayerType type) const;
    void AppendTerminatorLayer();
    void ChainLayers();
    void DumpLayerChain() const;

    bool inited_{false};

    std::vector<VulkanLayerPtr> layers_;
};

} // namespace OVS

#endif // VULKAN_LAYER_MANAGER_H