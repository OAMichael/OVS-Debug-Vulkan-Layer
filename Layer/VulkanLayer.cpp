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

void SerializeToStream(const GPUProfileInfo& info, WriteStream& stream) {
    uint64_t frameInfoSize = info.frameInfos.size();
    stream.Write(frameInfoSize);
    for (const auto& frameInfo : info.frameInfos) {
        SerializeToStream(frameInfo, stream);
    }
}

void SerializeToStream(const GPUProfileFrameInfo& info, WriteStream& stream) {
    stream.Write(info.frame);
    stream.Write(info.presentTimestamp);

    uint64_t commandBufferInfoSize = info.commandBufferInfos.size();
    stream.Write(commandBufferInfoSize);
    for (const auto& commandBufferInfo : info.commandBufferInfos) {
        SerializeToStream(commandBufferInfo, stream);
    }
}

void SerializeToStream(const GPUProfileCommandBufferInfo& info, WriteStream& stream) {
    stream.Write(info.commandBuffer);
    SerializeToStream(info.rootZone, stream);
}

void SerializeToStream(const GPUZone& info, WriteStream& stream) {
    uint64_t nameSize = info.name.size();
    stream.Write(nameSize);
    stream.Write(info.name.c_str(), nameSize * sizeof(char));

    stream.Write(info.begin);
    stream.Write(info.end);

    uint64_t childrenSize = info.children.size();
    stream.Write(childrenSize);
    for (const auto& child : info.children) {
        SerializeToStream(child, stream);
    }
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

void DeserializeFromStream(GPUProfileInfo& info, const ReadStream& stream) {
    uint64_t frameInfoSize = 0;
    stream.Read(frameInfoSize);
    info.frameInfos.resize(frameInfoSize);
    for (auto& frameInfo : info.frameInfos) {
        DeserializeFromStream(frameInfo, stream);
    }
}

void DeserializeFromStream(GPUProfileFrameInfo& info, const ReadStream& stream) {
    stream.Read(info.frame);
    stream.Read(info.presentTimestamp);

    uint64_t commandBufferInfoSize = 0;
    stream.Read(commandBufferInfoSize);
    info.commandBufferInfos.resize(commandBufferInfoSize);
    for (auto& commandBufferInfo : info.commandBufferInfos) {
        DeserializeFromStream(commandBufferInfo, stream);
    }
}

void DeserializeFromStream(GPUProfileCommandBufferInfo& info, const ReadStream& stream) {
    stream.Read(info.commandBuffer);
    DeserializeFromStream(info.rootZone, stream);
}

void DeserializeFromStream(GPUZone& info, const ReadStream& stream) {
    uint64_t nameSize = 0;
    stream.Read(nameSize);
    info.name.resize(nameSize);
    stream.Read(info.name.data(), nameSize * sizeof(char));

    stream.Read(info.begin);
    stream.Read(info.end);

    uint64_t childrenSize = 0;
    stream.Read(childrenSize);
    info.children.resize(childrenSize);
    for (auto& child : info.children) {
        DeserializeFromStream(child, stream);
    }
}

} // namespace OVS