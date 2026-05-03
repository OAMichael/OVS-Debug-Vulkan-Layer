#include <ShaderProfiler.h>
#include <VulkanLayer.h>
#include <VulkanShader.h>

#include <spirv-tools/libspirv.hpp>
#include <opt/build_module.h>
#include <spirv_constant.h>

#include <sstream>
#include <iostream>
#include <ranges>

static void SPIRVErrorHandler(spv_message_level_t, const char*, const spv_position_t&, const char* m) {
    std::cout << "Print SPIRV: " << m << '\n';
};

bool HandleShaderProfilerLayer(std::FILE* inputFile, std::FILE* outputFile) {
    OVS::ShaderProfilerFileHeader shaderProfHeader{};
    std::fread(&shaderProfHeader, sizeof(OVS::ShaderProfilerFileHeader), 1, inputFile);
    if (std::feof(inputFile)) {
        return false;
    }

    std::vector<uint8_t> data(shaderProfHeader.byteSize);
    std::fread(data.data(), sizeof(uint8_t), data.size(), inputFile);
    if (std::feof(inputFile)) {
        return false;
    }

    OVS::ReadStream stream(data);

    uint64_t collectedProfileInfoSize = 0;
    stream.Read(collectedProfileInfoSize);

    std::vector<OVS::CollectedPipelineProfileInfo> collectedProfileInfos(collectedProfileInfoSize);
    for (auto& collectedProfileInfo : collectedProfileInfos) {
        OVS::DeserializeFromStream(collectedProfileInfo, stream);
    }

    std::stringstream ss;
    for (const auto& profileInfo : collectedProfileInfos) {
        ss << "Pipeline " << profileInfo.pipeline << " (" << OVS::GetVulkanPipelineBindPointName(profileInfo.bindPoint) << "):\n";
        for (const auto& shaderInfo : profileInfo.shaderInfos) {
            const auto& profileData = shaderInfo.profileData;

            ss << "    Shader " << shaderInfo.shader << " (" << OVS::GetVulkanShaderStageName(shaderInfo.stage) << "): [";
            for (size_t i = 0; i < profileData.size(); ++i) {
                if (i > 0) {
                    ss << ", ";
                }
                ss << profileData[i];
            }
            ss << "]\n";

            auto context = spvtools::BuildModule(SPV_ENV_VULKAN_1_4, &SPIRVErrorHandler, shaderInfo.code.data(), shaderInfo.code.size(), true);
            if (!context) {
                std::cout << "SPIRV: Could not build module\n";
                continue;
            }

            auto& m = *context->module();

            OVS::SPIRVProfileInfo spvProfileInfo;
            OVS::SetupSPIRVProfileInfo(m, profileData, spvProfileInfo);

            if (OVS::ParseSPIRVDebugInfo(m, spvProfileInfo.debugInfo)) {
                OVS::ComputeLinesExecuted(m, spvProfileInfo);

                const auto& source = spvProfileInfo.debugInfo.source;
                auto lines = source | std::views::split('\n');

                size_t maxLineLen = 0;
                for (const auto& line : lines) {
                    maxLineLen = std::max(maxLineLen, line.size());
                }

                size_t lineNum = 1;
                std::string separator = std::string(maxLineLen, '=');

                ss << "        " << separator << '\n';
                for (const auto& line : lines) {
                    ss << "        " << std::string_view(line.data(), line.size());

                    auto lineExecutedIt = spvProfileInfo.linesExecuted.find(lineNum);
                    if (lineExecutedIt != spvProfileInfo.linesExecuted.end()) {
                        uint64_t lineExecuted = lineExecutedIt->second;
                        double lineExecutedPercent = 100.0 * double(lineExecuted) / double(spvProfileInfo.totalInstExecuted);

                        size_t spacingLength = maxLineLen - line.size() + 1;
                        std::string spacing(spacingLength, ' ');
                        ss << spacing << lineExecuted << " (" << lineExecutedPercent << "%)";
                    }

                    ss << '\n';
                    ++lineNum;
                }
                ss << "        " << separator << '\n';
            }

            constexpr uint32_t printFlags = SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES | SPV_BINARY_TO_TEXT_OPTION_NESTED_INDENT;
            std::vector<std::string> instsStr;
            size_t maxInstStrLen = 0;

            for (const auto& f : m) {
                for (const auto& bb : f) {
                    for (const auto& inst : bb) {
                        auto str = inst.PrettyPrint(printFlags);
                        maxInstStrLen = std::max(maxInstStrLen, str.length());
                        instsStr.emplace_back(std::move(str));
                    }
                }
            }

            size_t instIdx = 0;
            size_t bbIdx = 0;
            size_t funcIdx = 0;
            std::string separator = std::string(maxInstStrLen, '-');
            for (const auto& f : m) {
                uint64_t currFuncExecuted = spvProfileInfo.funcExecuted[f.result_id()];
                double currFuncExecutedPercent = 100.0 * double(currFuncExecuted) / double(spvProfileInfo.totalFuncExecuted);

                ss << "        Function #" << (funcIdx++) << ", id = " << f.result_id() << ": " << currFuncExecuted << " (" << currFuncExecutedPercent << "%)\n";
                for (const auto& bb : f) {
                    uint64_t currBBExecuted = spvProfileInfo.bbExecuted[bb.id()];
                    double currBBExecutedPercent = 100.0 * double(currBBExecuted) / double (spvProfileInfo.totalBBExecuted);

                    ss << "            Basic Block #" << (bbIdx++) << ", id = " << bb.id() << ": " << currBBExecuted << " (" << currBBExecutedPercent << "%)\n";
                    ss << "            " << separator << '\n';
                    for (const auto& inst : bb) {
                        const auto& instStr = instsStr[instIdx++];
                        uint64_t currInstExecuted = spvProfileInfo.instExecuted[inst.unique_id()];
                        double currInstExecutedPercent = 100.0 * double(currInstExecuted) / double(spvProfileInfo.totalInstExecuted);

                        size_t spacingLength = maxInstStrLen - instStr.length() + 1;
                        std::string spacing(spacingLength, ' ');
                        ss << "            " << instStr << spacing;

                        auto instLineIt = spvProfileInfo.debugInfo.instLines.find(inst.unique_id());
                        if (instLineIt != spvProfileInfo.debugInfo.instLines.end()) {
                            auto instLine = instLineIt->second;
                            ss << "Line " << instLine << ", ";
                        }

                        ss << currInstExecuted << " (" << currInstExecutedPercent << "%)\n";
                    }
                    ss << "            " << separator << "\n\n";
                }
            }
        }
        ss << "\n";
    }

    const auto& str = ss.str();
    std::fwrite(str.c_str(), sizeof(char), str.size(), outputFile);

    return true;
}
