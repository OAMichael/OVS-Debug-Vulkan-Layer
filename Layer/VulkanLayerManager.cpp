#include <VulkanLayerManager.h>
#include <VulkanLayerTerminator.h>
#include <VulkanLayerScreenshot.h>
#include <VulkanLayerPrinterGenerated.h>

#include <PlatformUtils.h>

#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace OVS {

void VulkanLayerManager::Init() {
    if (inited_) {
        return;
    }

    auto settingsPath = GetEnvVar("VK_OVS_DEBUG_SETTINGS_PATH");
    CreateLayersFromJSON(settingsPath);

    inited_ = true;
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
            std::string type = layerInfo["Type"];

            if (type == "Terminator") {
                auto layer = std::make_unique<VulkanLayerTerminator>();
                layers_.emplace_back(std::move(layer));
            }
            else if (type == "Printer") {
                VulkanLayerPrinterSettings settings{};
                if (layerInfo.contains("Settings")) {
                    const auto& settingsJSON = layerInfo["Settings"];
                    settings.filename = settingsJSON["Filename"];
                }

                auto layer = std::make_unique<VulkanLayerPrinter>(settings);
                layers_.emplace_back(std::move(layer));
            }
            else if (type == "Screenshot") {
                VulkanLayerScreenshotSettings settings{};
                if (layerInfo.contains("Settings")) {
                    const auto& settingsJSON = layerInfo["Settings"];
                    settings.fileBaseName = settingsJSON["FileBaseName"];

                    std::string frameRangesStr = settingsJSON["FrameRanges"];
                    if (!ParseFrameRanges(frameRangesStr, settings.frameRanges)) {
                        std::cout << "[DEBUG] Could not parse Screenshot Layer frame ranges\n";
                    }

                    auto layer = std::make_unique<VulkanLayerScreenshot>(settings);
                    layers_.emplace_back(std::move(layer));
                }
            }
        }

        for (int i = 0; i < layers_.size() - 1; ++i) {
            auto& curr = layers_[i];
            auto& next = layers_[i + 1];
            curr->SetNext(next.get());
        }
    }
    catch (nlohmann::json::exception e) {
        std::cout << "[DEBUG] Could not load settings JSON: " << e.what() << "\n";
        return false;
    }

    return true;
}

bool VulkanLayerManager::ParseFrameRanges(const std::string& frameRangesStr, std::vector<FrameRange>& out) const {
    std::vector<std::string> rangesStr;

    size_t findPos = 0;
    while (true) {
        auto pos = frameRangesStr.find(',', findPos);
        auto count = pos - findPos;
        if (pos == std::string::npos) {
            count = pos - frameRangesStr.size();
        }

        auto sub = frameRangesStr.substr(findPos, count);
        std::erase(sub, ' ');
        rangesStr.push_back(sub);

        if (pos == std::string::npos) {
            break;
        }

        findPos = pos + 1;
    }

    std::vector<FrameRange> frameRanges;
    for (const auto& rangeStr : rangesStr) {
        auto dash = rangeStr.find('-');
        if (dash == std::string::npos) {
            uint32_t frame = 0;

            auto begin = rangeStr.data();
            auto end = begin + rangeStr.size();
            auto res = std::from_chars(begin, end, frame);
            if (res.ec != std::errc()) {
                return false;
            }

            frameRanges.push_back(FrameRange(frame, frame));
        }
        else {
            if (dash == rangeStr.size() - 1) {
                return false;
            }

            uint32_t start = 0;
            uint32_t end = 0;

            auto begin1 = rangeStr.data();
            auto end1 = begin1 + dash;
            auto begin2 = begin1 + dash + 1;
            auto end2 = begin1 + rangeStr.size();
            auto res1 = std::from_chars(begin1, end1, start);
            auto res2 = std::from_chars(begin2, end2, end);
            if (res1.ec != std::errc() || res2.ec != std::errc()) {
                return false;
            }

            frameRanges.push_back(FrameRange(start, end));
        }
    }

    out = std::move(frameRanges);
    return true;
}

} // namespace OVS