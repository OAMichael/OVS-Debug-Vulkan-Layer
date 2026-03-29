#include <APITrace.h>
#include <VulkanLayer.h>
#include <SignatureGenerated.h>

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
