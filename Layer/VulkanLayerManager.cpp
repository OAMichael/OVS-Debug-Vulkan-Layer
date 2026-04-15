#include <VulkanLayerManager.h>
#include <VulkanLayerTerminator.h>
#include <VulkanLayerScreenshot.h>
#include <VulkanLayerOverlay.h>
#include <VulkanLayerPrinterGenerated.h>
#include <VulkanLayerAPIDumpGenerated.h>
#include <VulkanLayerAPITraceGenerated.h>
#include <VulkanLayerGPUProfiler.h>
#include <VulkanLayerShaderProfiler.h>
#include <VulkanLayerShaderOptimizer.h>

#include <PlatformUtils.h>

#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>


#include <SignatureGenerated.h>


namespace OVS {

void VulkanLayerManager::Init() {
    if (inited_) {
        return;
    }

    auto settingsPath = GetEnvVar("VK_OVS_DEBUG_SETTINGS_PATH");
    CreateLayersFromJSON(settingsPath);

    if (!ContainsLayer(VulkanLayerType::Terminator)) {
        std::cout << "[DEBUG] Layers chain does not contain Terminator Layer. Appending it to the end...\n";
        AppendTerminatorLayer();
    }

    ChainLayers();
    DumpLayerChain();

    inited_ = true;
}

void VulkanLayerManager::Cleanup() {
    if (!inited_) {
        return;
    }

    layers_.clear();

    inited_ = false;
}

bool VulkanLayerManager::CreateLayersFromJSON(const std::string& settingsPath) {
    if (settingsPath.empty()) {
        std::cout << "[DEBUG] Settings JSON path is empty\n";
        return false;
    }

    std::ifstream jfile(settingsPath);
    if (!jfile.is_open()) {
        std::cout << "[DEBUG] Could not open settings JSON file (\"" << settingsPath << "\")\n";
        return false;
    }

    try {
        auto data = nlohmann::json::parse(jfile);
        const auto& layers = data["Layers"];
        for (const auto& layerInfo : layers) {
            std::string typeStr = layerInfo["Type"];
            auto type = GetLayerTypeByName(typeStr);
            switch (type) {
                case VulkanLayerType::Terminator: {
                    auto layer = std::make_unique<VulkanLayerTerminator>();
                    layers_.emplace_back(std::move(layer));
                    break;
                }
                case VulkanLayerType::Printer: {
                    VulkanLayerPrinterSettings settings{};
                    if (layerInfo.contains("Settings")) {
                        const auto& settingsJSON = layerInfo["Settings"];
                        if (settingsJSON.contains("Filename")) {
                            settings.filename = settingsJSON["Filename"];
                        }
                    }

                    auto layer = std::make_unique<VulkanLayerPrinter>(settings);
                    layers_.emplace_back(std::move(layer));
                    break;
                }
                case VulkanLayerType::Screenshot: {
                    auto settings = VulkanLayerScreenshot::ParseSettingsFromJSON(layerInfo);
                    auto layer = std::make_unique<VulkanLayerScreenshot>(settings);
                    layers_.emplace_back(std::move(layer));
                    break;
                }
                case VulkanLayerType::Overlay: {
                    auto settings = VulkanLayerOverlay::ParseSettingsFromJSON(layerInfo);
                    auto layer = std::make_unique<VulkanLayerOverlay>(settings);
                    layers_.emplace_back(std::move(layer));
                    break;
                }
                case VulkanLayerType::APIDump: {
                    VulkanLayerAPIDumpSettings settings{};
                    if (layerInfo.contains("Settings")) {
                        const auto& settingsJSON = layerInfo["Settings"];
                        if (settingsJSON.contains("Filename")) {
                            settings.filename = settingsJSON["Filename"];
                        }
                    }

                    auto layer = std::make_unique<VulkanLayerAPIDump>(settings);
                    layers_.emplace_back(std::move(layer));
                    break;
                }
                case VulkanLayerType::APITrace: {
                    VulkanLayerAPITraceSettings settings{};
                    if (layerInfo.contains("Settings")) {
                        const auto& settingsJSON = layerInfo["Settings"];
                        if (settingsJSON.contains("Filename")) {
                            settings.filename = settingsJSON["Filename"];
                        }
                        if (settingsJSON.contains("FlushSize")) {
                            settings.flushSize = settingsJSON["FlushSize"];
                        }
                    }

                    auto layer = std::make_unique<VulkanLayerAPITrace>(settings);
                    layers_.emplace_back(std::move(layer));
                    break;
                }
                case VulkanLayerType::GPUProfiler: {
                    auto settings = VulkanLayerGPUProfiler::ParseSettingsFromJSON(layerInfo);
                    auto layer = std::make_unique<VulkanLayerGPUProfiler>(settings);
                    layers_.emplace_back(std::move(layer));
                    break;
                }
                case VulkanLayerType::ShaderProfiler: {
                    auto settings = VulkanLayerShaderProfiler::ParseSettingsFromJSON(layerInfo);
                    auto layer = std::make_unique<VulkanLayerShaderProfiler>(settings);
                    layers_.emplace_back(std::move(layer));
                    break;
                }
                case VulkanLayerType::ShaderOptimizer: {
                    auto settings = VulkanLayerShaderOptimizer::ParseSettingsFromJSON(layerInfo);
                    auto layer = std::make_unique<VulkanLayerShaderOptimizer>(settings);
                    layers_.emplace_back(std::move(layer));
                    break;
                }
                default: {
                    std::cout << "[DEBUG] Unknown Layer Type: \"" << typeStr << "\". Skipping...\n";
                    break;
                }
            }
        }
    }
    catch (nlohmann::json::exception e) {
        std::cout << "[DEBUG] Could not load settings JSON: " << e.what() << "\n";
        return false;
    }

    return true;
}

bool VulkanLayerManager::ContainsLayer(VulkanLayerType type) const {
    for (const auto& layer : layers_) {
        if (layer->GetType() == type) {
            return true;
        }
    }
    return false;
}

void VulkanLayerManager::AppendTerminatorLayer() {
    auto layer = std::make_unique<VulkanLayerTerminator>();
    layers_.emplace_back(std::move(layer));
}

void VulkanLayerManager::ChainLayers() {
    for (int i = 0; i < layers_.size() - 1; ++i) {
        auto& curr = layers_[i];
        auto& next = layers_[i + 1];
        curr->SetNext(next.get());
    }
}

void VulkanLayerManager::DumpLayerChain() const {
    std::cout << "[DEBUG] VK_LAYER_OVS_DEBUG Layers\n";
    for (const auto& layer : layers_) {
        auto type = layer->GetType();
        auto name = GetLayerTypeName(type);

        std::cout << "[DEBUG] ||\n";
        std::cout << "[DEBUG] " << name << "\n";
    }
}

} // namespace OVS