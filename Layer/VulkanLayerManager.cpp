#include <VulkanLayerManager.h>
#include <VulkanLayerTerminator.h>
#include <VulkanLayerPrinterGenerated.h>

namespace OVS {

void VulkanLayerManager::Init() {
    if (inited_) {
        return;
    }

    VulkanLayerPtr terminatorLayer = std::make_unique<VulkanLayerTerminator>();
    VulkanLayerPtr printerLayer = std::make_unique<VulkanLayerPrinter>();

    printerLayer->SetNext(terminatorLayer.get());

    layers_.emplace_back(std::move(printerLayer));
    layers_.emplace_back(std::move(terminatorLayer));

    inited_ = true;
}

} // namespace OVS