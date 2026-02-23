#include <iostream>
#include <fstream>
#include <string>

#include <SignatureGenerated.h>
#include <VulkanLayer.h>

using SignatureHeader = OVS::SignatureSerializer::SignatureHeader;

bool HandleAPITraceLayer(std::FILE* inputFile, std::FILE* outputFile) {
    constexpr long sigHeaderSize = sizeof(SignatureHeader);

    OVS::APITraceFileHeader apitraceHeader{};
    std::fread(&apitraceHeader, sizeof(OVS::APITraceFileHeader), 1, inputFile);
    if (std::feof(inputFile)) {
        return false;
    }

    SignatureHeader sigHeader{};
    std::vector<uint8_t> sigData;
    for (uint64_t i = 0; i < apitraceHeader.signatureCount; ++i) {
        std::fread(&sigHeader, sigHeaderSize, 1, inputFile);
        if (std::feof(inputFile)) {
            return false;
        }

        std::fseek(inputFile, -sigHeaderSize, SEEK_CUR);

        sigData.resize(sigHeader.byteSize);
        std::fread(sigData.data(), sigHeader.byteSize, 1, inputFile);
        if (std::feof(inputFile)) {
            return false;
        }

        auto sig = OVS::SignatureSerializer::CreateSignature(sigHeader.callID);

        OVS::ReadStream stream(sigData);
        sig->DeserializeFromStream(stream);

        std::stringstream ss;
        sig->SerializeToString(ss);

        std::string str = ss.str();
        str.push_back('\n');

        std::fwrite(str.c_str(), 1, str.size(), outputFile);
    }

    return true;
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
        }
        ss << "\n";
    }

    const auto& str = ss.str();
    std::fwrite(str.c_str(), sizeof(char), str.size(), outputFile);

    return true;
}


int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <InputFilename> [OutputFilename]" << std::endl;
        return 0;
    }

    const char* inputFilename  = argv[1];
    std::FILE* inputFile = std::fopen(inputFilename, "rb");
    if (!inputFile) {
        std::cout << "Could not open file \"" << inputFilename << "\"" << std::endl;
        return 0;
    }

    OVS::OVSFileHeader ovsHeader{};
    std::fread(&ovsHeader, sizeof(OVS::OVSFileHeader), 1, inputFile);
    if (ovsHeader.magic != OVS::OVSFileMagic) {
        std::cout << "File \"" << inputFilename << "\" is invalid OVS file (incorrect magic)" << std::endl;
        std::fclose(inputFile);
        return 0;
    }

    if (ovsHeader.version != OVS::OVSFileVersion) {
        std::cout << "File \"" << inputFilename << "\" is invalid OVS file (incorrect version)" << std::endl;
        std::fclose(inputFile);
        return 0;
    }

    uint32_t layerTypeU32 = ovsHeader.layerType;
    if (layerTypeU32 == uint32_t(OVS::VulkanLayerType::None) || layerTypeU32 >= uint32_t(OVS::VulkanLayerType::Count)) {
        std::cout << "File \"" << inputFilename << "\" is invalid OVS file (incorrect layer type)" << std::endl;
        std::fclose(inputFile);
        return 0;
    }

    bool handled = false;
    OVS::VulkanLayerType layerType = OVS::VulkanLayerType(layerTypeU32);
    switch (layerType) {
        case OVS::VulkanLayerType::APITrace: {
            if (argc < 3) {
                std::cout << "Output file is required for layer type \"" << OVS::GetLayerTypeName(layerType) << "\"" << std::endl;
                break;
            }

            const char* outputFilename = argv[2];
            std::FILE* outputFile = std::fopen(outputFilename, "w");
            if (!outputFile) {
                std::cout << "Could not open file \"" << outputFilename << "\"" << std::endl;
                break;
            }

            handled = HandleAPITraceLayer(inputFile, outputFile);

            std::fclose(outputFile);
            break;
        }
        case OVS::VulkanLayerType::ShaderProfiler: {
            if (argc < 3) {
                std::cout << "Output file is required for layer type \"" << OVS::GetLayerTypeName(layerType) << "\"" << std::endl;
                break;
            }

            const char* outputFilename = argv[2];
            std::FILE* outputFile = std::fopen(outputFilename, "w");
            if (!outputFile) {
                std::cout << "Could not open file \"" << outputFilename << "\"" << std::endl;
                break;
            }

            handled = HandleShaderProfilerLayer(inputFile, outputFile);

            std::fclose(outputFile);
            break;
        }
        default: {
            std::cout << "File \"" << inputFilename << "\" is valid OVS file but it is of type \"" << OVS::GetLayerTypeName(layerType) << "\" which can't be handled by this tool" << std::endl;
            break;
        }
    }

    if (!handled) {
        std::cout << "Could not handle file \"" << inputFilename << "\" of type \"" << OVS::GetLayerTypeName(layerType) << "\"" << std::endl;
    }

    std::fclose(inputFile);
    return 0;
}