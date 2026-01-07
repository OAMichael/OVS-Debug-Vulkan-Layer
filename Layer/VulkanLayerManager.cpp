#include <VulkanLayerManager.h>
#include <VulkanLayerTerminator.h>
#include <VulkanLayerScreenshot.h>
#include <VulkanLayerPrinterGenerated.h>

namespace OVS {

void VulkanLayerManager::Init() {
    if (inited_) {
        return;
    }

    auto terminatorLayer = std::make_unique<VulkanLayerTerminator>();
    auto printerLayer    = std::make_unique<VulkanLayerPrinter>();
    auto screenshotLayer = std::make_unique<VulkanLayerScreenshot>();

    printerLayer->SetNext(terminatorLayer.get());
    screenshotLayer->SetNext(printerLayer.get());

    screenshotLayer->SetFileBaseName("cube");
    screenshotLayer->AddFrameRange(FrameRange(10, 13));
    screenshotLayer->AddFrameRange(FrameRange(42, 42));
    screenshotLayer->AddFrameRange(FrameRange(555, 666));
    screenshotLayer->AddFrameRange(FrameRange(1000, 1024));

    layers_.emplace_back(std::move(screenshotLayer));
    layers_.emplace_back(std::move(printerLayer));
    layers_.emplace_back(std::move(terminatorLayer));

    inited_ = true;
}

} // namespace OVS