#include <GPUProfiler.h>
#include <VulkanLayer.h>

#include <sstream>
#include <iostream>

static void SerializeGPUZoneInfo(std::stringstream& ss, int depth, const OVS::GPUZone& zone, float timestampPeriod) {
    for (int i = 0; i < depth; ++i) {
        ss << "    ";
    }

    uint64_t tsLength = zone.end - zone.begin;
    float nsLength = tsLength * timestampPeriod;
    ss << zone.name << " (" << nsLength << " ns)\n";
    for (const auto& child : zone.children) {
        SerializeGPUZoneInfo(ss, depth + 1, child, timestampPeriod);
    }
}

bool HandleGPUProfilerLayer(std::FILE* inputFile, std::FILE* outputFile) {
    OVS::GPUProfilerFileHeader gpuProfHeader{};
    std::fread(&gpuProfHeader, sizeof(OVS::GPUProfilerFileHeader), 1, inputFile);
    if (std::feof(inputFile)) {
        return false;
    }

    std::vector<uint8_t> data(gpuProfHeader.byteSize);
    std::fread(data.data(), sizeof(uint8_t), data.size(), inputFile);
    if (std::feof(inputFile)) {
        return false;
    }

    OVS::ReadStream stream(data);

    OVS::GPUProfileInfo profileInfo;
    OVS::DeserializeFromStream(profileInfo, stream);

    constexpr size_t FrameSeparatorWidth = 50;
    constexpr const char FrameString[] = "Frame";
    std::string FrameSeparatorBottom(FrameSeparatorWidth, '=');

    std::stringstream ss;
    for (const auto& frameInfo : profileInfo.frameInfos) {
        uint32_t frame = frameInfo.frame;
        std::string frameStr = std::to_string(frame);
        size_t frameStrWidth = frameStr.size();
        size_t lSeparatorWidth = (FrameSeparatorWidth - 2 - sizeof(FrameString) - frameStrWidth) / 2;
        size_t rSeparatorWidth = (FrameSeparatorWidth - 2 - sizeof(FrameString) - frameStrWidth - lSeparatorWidth);
        std::string lSeparator(lSeparatorWidth, '=');
        std::string rSeparator(rSeparatorWidth, '=');
        std::string frameSeparatorUp = lSeparator + " Frame " + frameStr + " " + rSeparator;

        ss << frameSeparatorUp << '\n';

        for (uint32_t i = 0; i < frameInfo.commandBufferInfos.size(); ++i) {
            const auto& commandBufferInfo = frameInfo.commandBufferInfos[i];
            const auto& rootZone = commandBufferInfo.rootZone;
            SerializeGPUZoneInfo(ss, 0, rootZone, gpuProfHeader.timestampPeriod);
            if (i != frameInfo.commandBufferInfos.size() - 1) {
                ss << '\n';
            }
        }

        ss << FrameSeparatorBottom << '\n';
    }

    const auto& str = ss.str();
    std::fwrite(str.c_str(), sizeof(char), str.size(), outputFile);

    return true;
}
