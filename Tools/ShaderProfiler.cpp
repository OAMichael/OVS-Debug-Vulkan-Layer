#include <ShaderProfiler.h>
#include <VulkanLayer.h>

#include <spirv-tools/libspirv.hpp>
#include <opt/build_module.h>
#include <spirv_constant.h>

#include <sstream>
#include <iostream>

struct SPIRVDebugInfo {
    std::string filename;
    std::string source;
};

static void SPIRVErrorHandler(spv_message_level_t, const char*, const spv_position_t&, const char* m) {
    std::cout << "SPIRV: " << m << '\n';
};

bool ParseSPIRVDebugInfo(const spvtools::opt::Module& m, SPIRVDebugInfo& debugInfoOut) {
    SPIRVDebugInfo debugInfo;

    std::unordered_map<uint32_t, std::string> stringMap;
    for (const auto& inst : m.debugs1()) {
        switch (inst.opcode()) {
            case spv::Op::OpString: {
                if (inst.NumOperands() == 2) {
                    const auto& operand = inst.GetOperand(1);
                    uint32_t resultId = inst.result_id();
                    std::string str = operand.AsString();
                    stringMap[resultId] = std::move(str);
                }
                break;
            }
            case spv::Op::OpSource: {
                if (inst.NumOperands() == 4) {
                    const auto& operandFileId = inst.GetOperand(2);
                    const auto& operandSource = inst.GetOperand(3);

                    uint32_t fileId = operandFileId.AsId();
                    auto stringIt = stringMap.find(fileId);
                    if (stringIt != stringMap.end()) {
                        const auto& filename = stringIt->second;
                        debugInfo.filename = filename;
                    }

                    std::string source = operandSource.AsString();

                    const char lineString[] = "#line 1";
                    auto pos = source.find(lineString);
                    if (pos != std::string::npos) {
                        source = source.substr(pos + sizeof(lineString));
                    }

                    debugInfo.source = std::move(source);
                }
                break;
            }
            case spv::Op::OpSourceContinued: {
                if (inst.NumOperands() == 1) {
                    const auto& operandSource = inst.GetOperand(0);

                    std::string sourceContinued = operandSource.AsString();
                    debugInfo.source.append(sourceContinued);
                }
                break;
            }
            default: {
                break;
            }
        }
    }

    debugInfoOut = std::move(debugInfo);
    return !debugInfoOut.source.empty();
}

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

            SPIRVDebugInfo debugInfo;
            if (!ParseSPIRVDebugInfo(m, debugInfo)) {
                // TODO
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
            std::string separator = std::string(maxInstStrLen, '-');
            for (const auto& f : m) {
                for (const auto& bb : f) {
                    uint64_t execCount = profileData[bbIdx];
                    ss << "Basic Block #" << bbIdx << ": " << execCount << '\n';
                    ss << separator << '\n';
                    for (const auto& inst : bb) {
                        const auto& instStr = instsStr[instIdx];
                        ss << instStr << '\n';
                        ++instIdx;
                    }
                    ss << separator << "\n\n";
                    ++bbIdx;
                }
            }
        }
        ss << "\n";
    }

    const auto& str = ss.str();
    std::fwrite(str.c_str(), sizeof(char), str.size(), outputFile);

    return true;
}
