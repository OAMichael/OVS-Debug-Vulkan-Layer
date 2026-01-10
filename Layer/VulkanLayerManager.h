#ifndef VULKAN_LAYER_MANAGER_H
#define VULKAN_LAYER_MANAGER_H

#include <VulkanLayerInterfaceGenerated.h>

#include <vector>

namespace OVS {

class VulkanLayerManager {
public:
    void Init();

    inline VulkanLayerPtr& GetFrontLayer() { return layers_.front(); }

private:
    bool CreateLayersFromJSON(const std::string& settingsPath);
    bool ParseFrameRanges(const std::string& frameRangesStr, std::vector<FrameRange>& out) const;

    bool inited_{false};

    std::vector<VulkanLayerPtr> layers_;
};

} // namespace OVS

#endif // VULKAN_LAYER_MANAGER_H