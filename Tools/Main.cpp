#include <iostream>
#include <fstream>
#include <string>

#include <VulkanLayer.h>

#include <APITrace.h>
#include <ShaderProfiler.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <InputFilename> [OutputFilename]" << std::endl;
        return 0;
    }

    const char* inputFilename = argv[1];
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

    std::FILE* outputFile = nullptr;
    OVS::VulkanLayerType layerType = OVS::VulkanLayerType(layerTypeU32);
    if (layerType == OVS::VulkanLayerType::APITrace || layerType == OVS::VulkanLayerType::ShaderProfiler) {
        if (argc < 3) {
            std::cout << "Output file is required for layer type \"" << OVS::GetLayerTypeName(layerType) << "\"" << std::endl;
            std::fclose(inputFile);
            return 0;
        }

        const char* outputFilename = argv[2];
        outputFile = std::fopen(outputFilename, "w");
        if (!outputFile) {
            std::cout << "Could not open file \"" << outputFilename << "\"" << std::endl;
            std::fclose(inputFile);
            return 0;
        }
    }

    bool handled = false;
    switch (layerType) {
        case OVS::VulkanLayerType::APITrace: {
            handled = HandleAPITraceLayer(inputFile, outputFile);
            break;
        }
        case OVS::VulkanLayerType::ShaderProfiler: {
            handled = HandleShaderProfilerLayer(inputFile, outputFile);
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

    if (outputFile) {
        std::fclose(outputFile);
    }
    if (inputFile) {
        std::fclose(inputFile);
    }
    return 0;
}