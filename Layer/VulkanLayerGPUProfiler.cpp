#include <VulkanLayerGPUProfiler.h>
#include <VulkanUtils.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

namespace OVS {

static inline std::string GetCommandBufferName(VkCommandBuffer commandBuffer) {
    std::stringstream ss;
    ss << "Command Buffer 0x";
    ss << std::hex << uint64_t(commandBuffer);
    return ss.str();
}

VulkanLayerGPUProfilerSettings VulkanLayerGPUProfiler::ParseSettingsFromJSON(const nlohmann::json& layerInfo)
{
    VulkanLayerGPUProfilerSettings settings{};
    if (layerInfo.contains("Settings")) {
        const auto& settingsJSON = layerInfo["Settings"];
        if (settingsJSON.contains("Filename")) {
            settings.filename = settingsJSON["Filename"];
        }
        if (settingsJSON.contains("UseZoneBarriers")) {
            settings.useZoneBarriers = settingsJSON["UseZoneBarriers"];
        }
    }
    return settings;
}

VulkanLayerGPUProfiler::VulkanLayerGPUProfiler(const VulkanLayerGPUProfilerSettings& settings) : VulkanLayerPassThrough(VulkanLayerType::GPUProfiler), settings_{settings}
{
    profileInfo_.frameInfos.emplace_back();
}

VulkanLayerGPUProfiler::~VulkanLayerGPUProfiler()
{
    for (auto& [device, deviceInfo] : deviceInfoMap_) {
        auto& commandBufferInfos = deviceInfo.commandBufferInfos;
        next_->vkDeviceWaitIdle(device);
        for (const auto& [commandBuffer, commandBufferInfo] : commandBufferInfos) {
            if (commandBufferInfo.state == CommandBufferState::Pending) {
                CollectCommandBufferInfo(commandBufferInfo);
            }
        }
        deviceInfo.DestroyQueryPools();
    }

    StripProfileInfo();
    SaveProfileInfo();
}

VkResult VulkanLayerGPUProfiler::vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    std::lock_guard lock(mutex_);

    VkResult res = next_->vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (res == VK_SUCCESS) {
        VkDevice device = *pDevice;

        auto& next = *next_;
        auto [deviceInfoIt, _] = deviceInfoMap_.emplace(device, std::ref(next));

        auto& deviceInfo = deviceInfoIt->second;
        deviceInfo.device = device;
        deviceInfo.physicalDevice = physicalDevice;

        VkPhysicalDeviceProperties properties{};
        next_->vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        timestampPeriod_ = properties.limits.timestampPeriod;

        uint32_t queueFamilyPropertyCount = 0;
        next_->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, nullptr);

        deviceInfo.queueFamilyProperties.resize(queueFamilyPropertyCount);
        next_->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, deviceInfo.queueFamilyProperties.data());
    }
    return res;
}

void VulkanLayerGPUProfiler::vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    std::lock_guard lock(mutex_);

    auto it = deviceInfoMap_.find(device);
    if (it != deviceInfoMap_.end()) {
        auto& deviceInfo = it->second;
        auto& commandBufferInfos = deviceInfo.commandBufferInfos;
        next_->vkDeviceWaitIdle(device);
        for (const auto& [commandBuffer, commandBufferInfo] : commandBufferInfos) {
            if (commandBufferInfo.state == CommandBufferState::Pending) {
                CollectCommandBufferInfo(commandBufferInfo);
            }
        }
        deviceInfo.DestroyQueryPools();

        for (const auto& [queue, _] : deviceInfo.queueInfos) {
            queueToDeviceMap_.erase(queue);
        }
        for (const auto& [commandBuffer, _] : deviceInfo.commandBufferInfos) {
            commandBufferToDeviceMap_.erase(commandBuffer);
        }
        deviceInfoMap_.erase(it);
    }
    next_->vkDestroyDevice(device, pAllocator);
}

void VulkanLayerGPUProfiler::vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue)
{
    std::lock_guard lock(mutex_);

    next_->vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    if (!pQueue || !*pQueue) {
        return;
    }

    auto it = deviceInfoMap_.find(device);
    if (it == deviceInfoMap_.end()) {
        return;
    }

    VkQueue queue = *pQueue;
    queueToDeviceMap_[queue] = device;

    auto& deviceInfo = it->second;
    const auto& queueFamilyProperties = deviceInfo.queueFamilyProperties[queueFamilyIndex];
    deviceInfo.queueInfos[queue].queueFamilyIndex = queueFamilyIndex;
    deviceInfo.queueInfos[queue].queueIndex = queueIndex;
    deviceInfo.queueInfos[queue].timestampValidBits = queueFamilyProperties.timestampValidBits;
}

void VulkanLayerGPUProfiler::vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue)
{
    std::lock_guard lock(mutex_);

    next_->vkGetDeviceQueue2(device, pQueueInfo, pQueue);
    if (!pQueueInfo || !pQueue || !*pQueue) {
        return;
    }

    auto it = deviceInfoMap_.find(device);
    if (it == deviceInfoMap_.end()) {
        return;
    }

    VkQueue queue = *pQueue;
    queueToDeviceMap_[queue] = device;

    auto& deviceInfo = it->second;
    const auto& queueFamilyProperties = deviceInfo.queueFamilyProperties[pQueueInfo->queueFamilyIndex];
    deviceInfo.queueInfos[queue].queueFamilyIndex = pQueueInfo->queueFamilyIndex;
    deviceInfo.queueInfos[queue].queueIndex = pQueueInfo->queueIndex;
    deviceInfo.queueInfos[queue].timestampValidBits = queueFamilyProperties.timestampValidBits;
}

VkResult VulkanLayerGPUProfiler::vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool)
{
    std::lock_guard lock(mutex_);

    return next_->vkCreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
}

void VulkanLayerGPUProfiler::vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool, const VkAllocationCallbacks* pAllocator)
{
    std::lock_guard lock(mutex_);

    auto deviceInfoIt = deviceInfoMap_.find(device);
    if (deviceInfoIt != deviceInfoMap_.end()) {
        auto& deviceInfo = deviceInfoIt->second;
        auto& commandPoolInfos = deviceInfo.commandPoolInfos;
        auto& commandBufferInfos = deviceInfo.commandBufferInfos;
        auto& commandBufferToDevice = commandBufferToDeviceMap_;

        auto commandPoolInfoIt = commandPoolInfos.find(commandPool);
        if (commandPoolInfoIt != commandPoolInfos.end()) {
            const auto& commandPoolInfo = commandPoolInfoIt->second;
            const auto& commandBuffers = commandPoolInfo.commandBuffers;

            for (const auto& commandBuffer : commandBuffers) {
                auto commandBufferInfoIt = commandBufferInfos.find(commandBuffer);
                if (commandBufferInfoIt != commandBufferInfos.end()) {
                    const auto& commandBufferInfo = commandBufferInfoIt->second;
                    if (commandBufferInfo.state == CommandBufferState::Pending) {
                        CollectCommandBufferInfo(commandBufferInfo);
                    }

                    deviceInfo.FreeQueryPool(commandBufferInfo.queryPool);
                    commandBufferInfos.erase(commandBufferInfoIt);
                }

                commandBufferToDevice.erase(commandBuffer);
            }

            commandPoolInfos.erase(commandPoolInfoIt);
        }
    }
    next_->vkDestroyCommandPool(device, commandPool, pAllocator);
}

VkResult VulkanLayerGPUProfiler::vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags)
{
    std::lock_guard lock(mutex_);

    auto deviceInfoIt = deviceInfoMap_.find(device);
    if (deviceInfoIt != deviceInfoMap_.end()) {
        auto& deviceInfo = deviceInfoIt->second;
        auto& commandPoolInfos = deviceInfo.commandPoolInfos;
        auto& commandBufferInfos = deviceInfo.commandBufferInfos;

        auto commandPoolInfoIt = commandPoolInfos.find(commandPool);
        if (commandPoolInfoIt != commandPoolInfos.end()) {
            const auto& commandPoolInfo = commandPoolInfoIt->second;
            const auto& commandBuffers = commandPoolInfo.commandBuffers;

            for (const auto& commandBuffer : commandBuffers) {
                auto commandBufferInfoIt = commandBufferInfos.find(commandBuffer);
                if (commandBufferInfoIt == commandBufferInfos.end()) {
                    continue;
                }

                auto& commandBufferInfo = commandBufferInfoIt->second;
                if (commandBufferInfo.state == CommandBufferState::Pending) {
                    CollectCommandBufferInfo(commandBufferInfo);
                }

                commandBufferInfo.state = CommandBufferState::Initial;
                commandBufferInfo.queryCount = 0;
                commandBufferInfo.rootZone = {};
                commandBufferInfo.zonesStack = {};
            }
        }
    }
    return next_->vkResetCommandPool(device, commandPool, flags);
}

VkResult VulkanLayerGPUProfiler::vkAllocateCommandBuffers(VkDevice device, const VkCommandBufferAllocateInfo* pAllocateInfo, VkCommandBuffer* pCommandBuffers)
{
    std::lock_guard lock(mutex_);

    VkResult res = next_->vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
    if (res == VK_SUCCESS) {
        VkCommandPool commandPool = pAllocateInfo->commandPool;
        uint32_t commandBufferCount = pAllocateInfo->commandBufferCount;

        auto deviceInfoIt = deviceInfoMap_.find(device);
        if (deviceInfoIt != deviceInfoMap_.end()) {
            auto& deviceInfo = deviceInfoIt->second;
            auto& commandPoolInfos = deviceInfo.commandPoolInfos;
            auto& commandBufferInfos = deviceInfo.commandBufferInfos;
            auto& commandBufferToDevice = commandBufferToDeviceMap_;

            auto& commandPoolInfo = commandPoolInfos[commandPool];
            auto& commandBuffers = commandPoolInfo.commandBuffers;

            for (uint32_t i = 0; i < commandBufferCount; ++i) {
                VkCommandBuffer commandBuffer = pCommandBuffers[i];
                VkQueryPool queryPool = deviceInfo.AllocateQueryPool();

                commandBufferToDevice.emplace(commandBuffer, device);
                commandBuffers.emplace(commandBuffer);

                auto& commandBufferInfo = commandBufferInfos[commandBuffer];
                commandBufferInfo.commandBuffer = commandBuffer;
                commandBufferInfo.queryPool = queryPool;
            }
        }
    }
    return res;
}

void VulkanLayerGPUProfiler::vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers)
{
    std::lock_guard lock(mutex_);

    auto deviceInfoIt = deviceInfoMap_.find(device);
    if (deviceInfoIt != deviceInfoMap_.end()) {
        auto& deviceInfo = deviceInfoIt->second;
        auto& commandPoolInfos = deviceInfo.commandPoolInfos;
        auto& commandBufferInfos = deviceInfo.commandBufferInfos;
        auto& commandBufferToDevice = commandBufferToDeviceMap_;

        auto& commandPoolInfo = commandPoolInfos[commandPool];
        auto& commandBuffers = commandPoolInfo.commandBuffers;

        for (uint32_t i = 0; i < commandBufferCount; ++i) {
            VkCommandBuffer commandBuffer = pCommandBuffers[i];

            auto commandBufferInfoIt = commandBufferInfos.find(commandBuffer);
            if (commandBufferInfoIt != commandBufferInfos.end()) {
                const auto& commandBufferInfo = commandBufferInfoIt->second;
                if (commandBufferInfo.state == CommandBufferState::Pending) {
                    CollectCommandBufferInfo(commandBufferInfo);
                }

                deviceInfo.FreeQueryPool(commandBufferInfo.queryPool);
                commandBufferInfos.erase(commandBufferInfoIt);
            }

            commandBufferToDevice.erase(commandBuffer);
            commandBuffers.erase(commandBuffer);
        }
    }
    next_->vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

VkResult VulkanLayerGPUProfiler::vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo)
{
    std::lock_guard lock(mutex_);

    CommandBufferInfo* pCommandBufferInfo = GetCommandBufferInfo(commandBuffer);
    if (pCommandBufferInfo) {
        auto& commandBufferInfo = *pCommandBufferInfo;
        if (commandBufferInfo.state == CommandBufferState::Pending) {
            CollectCommandBufferInfo(commandBufferInfo);
        }
    }

    VkResult res = next_->vkBeginCommandBuffer(commandBuffer, pBeginInfo);
    if (res == VK_SUCCESS && pCommandBufferInfo) {
        auto& commandBufferInfo = *pCommandBufferInfo;
        VkQueryPool queryPool = commandBufferInfo.queryPool;

        next_->vkCmdResetQueryPool(commandBuffer, queryPool, 0, MaxQueryPerCommandBuffer);
        commandBufferInfo.state = CommandBufferState::Recording;
        commandBufferInfo.queryCount = 0;
        commandBufferInfo.rootZone = {};
        commandBufferInfo.zonesStack = {};

        uint32_t query = commandBufferInfo.queryCount++;
        next_->vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, query);

        auto& rootZone = commandBufferInfo.rootZone;
        rootZone.name = GetCommandBufferName(commandBuffer);
        rootZone.queryBegin = query;
        commandBufferInfo.zonesStack.push(&rootZone);
    }
    return res;
}

VkResult VulkanLayerGPUProfiler::vkEndCommandBuffer(VkCommandBuffer commandBuffer)
{
    std::lock_guard lock(mutex_);

    CommandBufferInfo* pCommandBufferInfo = GetCommandBufferInfo(commandBuffer);
    if (pCommandBufferInfo) {
        auto& commandBufferInfo = *pCommandBufferInfo;
        VkQueryPool queryPool = commandBufferInfo.queryPool;

        uint32_t query = commandBufferInfo.queryCount++;
        next_->vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, query);

        auto& rootZone = commandBufferInfo.rootZone;
        rootZone.queryEnd = query;
        commandBufferInfo.zonesStack.pop();
    }

    VkResult res = next_->vkEndCommandBuffer(commandBuffer);
    if (res == VK_SUCCESS && pCommandBufferInfo) {
        pCommandBufferInfo->state = CommandBufferState::Executable;
    }
    return res;
}

VkResult VulkanLayerGPUProfiler::vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags)
{
    std::lock_guard lock(mutex_);

    CommandBufferInfo* pCommandBufferInfo = GetCommandBufferInfo(commandBuffer);
    if (pCommandBufferInfo) {
        auto& commandBufferInfo = *pCommandBufferInfo;
        if (commandBufferInfo.state == CommandBufferState::Pending) {
            CollectCommandBufferInfo(commandBufferInfo);
        }

        commandBufferInfo.state = CommandBufferState::Initial;
        commandBufferInfo.queryCount = 0;
        commandBufferInfo.rootZone = {};
        commandBufferInfo.zonesStack = {};
    }
    return next_->vkResetCommandBuffer(commandBuffer, flags);
}

void VulkanLayerGPUProfiler::vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
    std::lock_guard lock(mutex_);

    if (settings_.useZoneBarriers) {
        // TODO: Make correct usage with subpass dependency
        next_->vkCmdPipelineBarrier(commandBuffer,
                                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    0,
                                    0, nullptr,
                                    0, nullptr,
                                    0, nullptr);
    }

    next_->vkCmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo);

    CommandBufferInfo* pCommandBufferInfo = GetCommandBufferInfo(commandBuffer);
    if (pCommandBufferInfo) {
        auto& commandBufferInfo = *pCommandBufferInfo;
        VkQueryPool queryPool = commandBufferInfo.queryPool;
        if (commandBufferInfo.queryCount == MaxQueryPerCommandBuffer) {
            std::cout << "GPUProfiler: Maximum timestamp id is reached while begining zone \"" << pLabelInfo->pLabelName << "\" at frame " << currentFrame_ << '\n';
        }
        else {
            uint32_t query = commandBufferInfo.queryCount++;
            next_->vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, query);

            auto& zonesStack = commandBufferInfo.zonesStack;
            auto& parentZone = *zonesStack.top();
            auto& zone = parentZone.children.emplace_back();
            zone.name = pLabelInfo->pLabelName;
            zone.queryBegin = query;
            zonesStack.push(&zone);
        }
    }
}

void VulkanLayerGPUProfiler::vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer)
{
    std::lock_guard lock(mutex_);

    CommandBufferInfo* pCommandBufferInfo = GetCommandBufferInfo(commandBuffer);
    if (pCommandBufferInfo) {
        auto& commandBufferInfo = *pCommandBufferInfo;
        VkQueryPool queryPool = commandBufferInfo.queryPool;
        if (commandBufferInfo.queryCount == MaxQueryPerCommandBuffer) {
            std::cout << "GPUProfiler: Maximum timestamp id is reached while ending zone at frame " << currentFrame_ << '\n';
        }
        else {
            uint32_t query = commandBufferInfo.queryCount++;
            next_->vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, query);

            auto& zoneStack = commandBufferInfo.zonesStack;
            auto& zone = *zoneStack.top();
            zone.queryEnd = query;
            zoneStack.pop();
        }
    }

    next_->vkCmdEndDebugUtilsLabelEXT(commandBuffer);
}

VkResult VulkanLayerGPUProfiler::vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence)
{
    std::lock_guard lock(mutex_);

    std::unordered_set<CommandBufferInfo*> pCommandBufferInfos;
    for (uint32_t i = 0; i < submitCount; ++i) {
        const auto& submitInfo = pSubmits[i];
        for (uint32_t j = 0; j < submitInfo.commandBufferCount; ++j) {
            const auto& commandBuffer = submitInfo.pCommandBuffers[j];
            CommandBufferInfo* pCommandBufferInfo = GetCommandBufferInfo(commandBuffer);
            if (pCommandBufferInfo) {
                pCommandBufferInfos.insert(pCommandBufferInfo);

                auto& commandBufferInfo = *pCommandBufferInfo;
                if (commandBufferInfo.state == CommandBufferState::Pending) {
                    CollectCommandBufferInfo(commandBufferInfo);
                    commandBufferInfo.state = CommandBufferState::Executable;
                }
            }
        }
    }

    VkResult res = next_->vkQueueSubmit(queue, submitCount, pSubmits, fence);
    if (res == VK_SUCCESS) {
        for (auto* pCommandBufferInfo : pCommandBufferInfos) {
            auto& commandBufferInfo = *pCommandBufferInfo;
            if (commandBufferInfo.state == CommandBufferState::Executable) {
                commandBufferInfo.state = CommandBufferState::Pending;
                commandBufferInfo.submitFrame = currentFrame_;
            }
        }
    }
    return res;
}

VkResult VulkanLayerGPUProfiler::vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence)
{
    std::lock_guard lock(mutex_);

    std::unordered_set<CommandBufferInfo*> pCommandBufferInfos;
    for (uint32_t i = 0; i < submitCount; ++i) {
        const auto& submitInfo = pSubmits[i];
        for (uint32_t j = 0; j < submitInfo.commandBufferInfoCount; ++j) {
            const auto& commandBufferInfo = submitInfo.pCommandBufferInfos[j];
            const auto& commandBuffer = commandBufferInfo.commandBuffer;
            CommandBufferInfo* pCommandBufferInfo = GetCommandBufferInfo(commandBuffer);
            if (pCommandBufferInfo) {
                pCommandBufferInfos.insert(pCommandBufferInfo);

                auto& commandBufferInfo = *pCommandBufferInfo;
                if (commandBufferInfo.state == CommandBufferState::Pending) {
                    CollectCommandBufferInfo(commandBufferInfo);
                    commandBufferInfo.state = CommandBufferState::Executable;
                }
            }
        }
    }

    VkResult res = next_->vkQueueSubmit2(queue, submitCount, pSubmits, fence);
    if (res == VK_SUCCESS) {
        for (auto* pCommandBufferInfo : pCommandBufferInfos) {
            auto& commandBufferInfo = *pCommandBufferInfo;
            if (commandBufferInfo.state == CommandBufferState::Executable) {
                commandBufferInfo.state = CommandBufferState::Pending;
                commandBufferInfo.submitFrame = currentFrame_;
            }
        }
    }
    return res;
}

VkResult VulkanLayerGPUProfiler::vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence)
{
    return vkQueueSubmit2(queue, submitCount, pSubmits, fence);
}

VkResult VulkanLayerGPUProfiler::vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    VkResult res = next_->vkQueuePresentKHR(queue, pPresentInfo);
    if (res == VK_SUCCESS) {
        profileInfo_.frameInfos.emplace_back();
        ++currentFrame_;
    }
    return res;
}

bool VulkanLayerGPUProfiler::CollectCommandBufferInfo(const CommandBufferInfo& commandBufferInfo)
{
    if (commandBufferInfo.queryCount == 0) {
        return true;
    }

    auto deviceIt = commandBufferToDeviceMap_.find(commandBufferInfo.commandBuffer);
    if (deviceIt == commandBufferToDeviceMap_.end()) {
        return false;
    }

    VkDevice device = deviceIt->second;
    VkQueryPool queryPool = commandBufferInfo.queryPool;
    uint32_t firstQuery = 0;
    uint32_t queryCount = commandBufferInfo.queryCount;
    size_t dataSize = queryCount * sizeof(uint64_t);

    constexpr VkDeviceSize stride = sizeof(uint64_t);
    constexpr VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT;

    std::vector<uint64_t> queryResults(queryCount);
    VkResult res = next_->vkGetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, queryResults.data(), stride, flags);
    if (res != VK_SUCCESS) {
        return false;
    }

    uint32_t frame = commandBufferInfo.submitFrame;
    VkCommandBuffer commandBuffer = commandBufferInfo.commandBuffer;

    auto& profileInfo = profileInfo_;
    auto& profileFrameInfo = profileInfo.frameInfos[frame - 1];
    auto& profileCommandBufferInfo = profileFrameInfo.commandBufferInfos.emplace_back();

    profileFrameInfo.frame = frame;
    profileCommandBufferInfo.commandBuffer = commandBufferInfo.commandBuffer;

    const auto& rawRootZone = commandBufferInfo.rootZone;
    auto& profileRootZone = profileCommandBufferInfo.rootZone;
    ConvertGPUZones(queryResults, rawRootZone, profileRootZone);

    return true;
}

void VulkanLayerGPUProfiler::ConvertGPUZones(const std::vector<uint64_t>& queryResults, const GPUZoneInfo& rawZone, GPUZone& profileZone)
{
    profileZone.name = std::move(rawZone.name);
    profileZone.begin = queryResults[rawZone.queryBegin];
    profileZone.end = queryResults[rawZone.queryEnd];

    const auto& rawChildren = rawZone.children;
    auto& profileChildren = profileZone.children;

    size_t childrenSize = rawChildren.size();
    profileChildren.resize(childrenSize);
    for (size_t i = 0; i < childrenSize; ++i) {
        const auto& rawChild = rawChildren[i];
        auto& profileChild = profileChildren[i];
        ConvertGPUZones(queryResults, rawChild, profileChild);
    }
}

void VulkanLayerGPUProfiler::StripProfileInfo()
{
    auto& profileInfo = profileInfo_;
    auto& frameInfos = profileInfo.frameInfos;
    if (frameInfos.empty()) {
        return;
    }

    while (frameInfos.back().frame == 0) {
        frameInfos.pop_back();
    }
}

bool VulkanLayerGPUProfiler::SaveProfileInfo() const
{
    std::vector<uint8_t> data;
    WriteStream stream(data);
    SerializeToStream(profileInfo_, stream);

    const auto& filename = settings_.filename;
    std::FILE* perfFile = std::fopen(filename.c_str(), "wb");
    if (!perfFile) {
        std::cout << "VulkanLayerGPUProfiler::SaveProfileInfo: Could not open file: \'" << filename << "\'\n";
        return false;
    }

    OVSFileHeader ovsHeader{};
    ovsHeader.layerType = uint32_t(VulkanLayerType::GPUProfiler);
    std::fwrite(&ovsHeader, sizeof(OVSFileHeader), 1, perfFile);

    GPUProfilerFileHeader gpuProfHeader{};
    gpuProfHeader.byteSize = data.size();
    gpuProfHeader.timestampPeriod = timestampPeriod_;
    std::fwrite(&gpuProfHeader, sizeof(GPUProfilerFileHeader), 1, perfFile);

    std::fwrite(data.data(), sizeof(uint8_t), data.size(), perfFile);

    std::fclose(perfFile);
    return true;
}

} // namespace OVS