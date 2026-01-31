#include <iostream>
#include <fstream>
#include <string>

#include <SignatureGenerated.h>


int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <InputFilename> <OutputFilename>" << std::endl;
        return 0;
    }

    const char* inputFilename  = argv[1];
    const char* outputFilename = argv[2];
    std::FILE* inputFile  = std::fopen(inputFilename, "rb");
    if (!inputFile) {
        std::cout << "Could not open file \"" << inputFilename << "\"" << std::endl;
        return 0;
    }

    std::FILE* outputFile = std::fopen(outputFilename, "w");
    if (!outputFile) {
        std::cout << "Could not open file \"" << outputFilename << "\"" << std::endl;
        std::fclose(inputFile);
        return 0;
    }

    using SignatureHeader = OVS::SignatureSerializer::SignatureHeader;

    constexpr long headerSize = sizeof(SignatureHeader);

    SignatureHeader header{};
    std::vector<uint8_t> data;
    while (true) {
        std::fread(&header, headerSize, 1, inputFile);
        if (std::feof(inputFile)) {
            break;
        }

        std::fseek(inputFile, -headerSize, SEEK_CUR);

        data.resize(header.byteSize);
        std::fread(data.data(), header.byteSize, 1, inputFile);
        if (std::feof(inputFile)) {
            break;
        }

        auto sig = OVS::SignatureSerializer::CreateSignature(header.callID);

        OVS::SignatureSerializer::ReadStream stream(data);
        sig->DeserializeFromStream(stream);

        std::stringstream ss;
        sig->SerializeToString(ss);

        std::string str = ss.str();
        str.push_back('\n');

        std::fwrite(str.c_str(), 1, str.size(), outputFile);
    }

    std::fclose(outputFile);
    std::fclose(inputFile);
    return 0;
}