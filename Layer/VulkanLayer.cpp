#include <VulkanLayer.h>

namespace OVS {

void SerializeToStream(const CollectedPipelineProfileInfo& info, WriteStream& stream) {
    stream.Write(info.bindPoint);
    stream.Write(info.pipeline);

    uint64_t shaderInfoSize = info.shaderInfos.size();
    stream.Write(shaderInfoSize);
    for (const auto& shaderInfo : info.shaderInfos) {
        SerializeToStream(shaderInfo, stream);
    }
}

void SerializeToStream(const CollectedShaderProfileInfo& info, WriteStream& stream) {
    stream.Write(info.stage);
    stream.Write(info.shader);

    uint64_t codeSize = info.code.size();
    stream.Write(codeSize);
    stream.Write(info.code.data(), codeSize * sizeof(uint32_t));

    uint64_t profileDataSize = info.profileData.size();
    stream.Write(profileDataSize);
    stream.Write(info.profileData.data(), profileDataSize * sizeof(uint64_t));
}

void DeserializeFromStream(CollectedPipelineProfileInfo& info, const ReadStream& stream) {
    stream.Read(info.bindPoint);
    stream.Read(info.pipeline);

    uint64_t shaderInfoSize = 0;
    stream.Read(shaderInfoSize);
    info.shaderInfos.resize(shaderInfoSize);
    for (auto& shaderInfo : info.shaderInfos) {
        DeserializeFromStream(shaderInfo, stream);
    }
}

void DeserializeFromStream(CollectedShaderProfileInfo& info, const ReadStream& stream) {
    stream.Read(info.stage);
    stream.Read(info.shader);

    uint64_t codeSize = 0;
    stream.Read(codeSize);
    info.code.resize(codeSize);
    stream.Read(info.code.data(), codeSize * sizeof(uint32_t));

    uint64_t profileDataSize = 0;
    stream.Read(profileDataSize);
    info.profileData.resize(profileDataSize);
    stream.Read(info.profileData.data(), profileDataSize * sizeof(uint64_t));
}

} // namespace OVS