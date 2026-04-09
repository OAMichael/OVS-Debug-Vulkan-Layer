#include <VulkanUtils.h>
#include <VulkanLayerScreenshot.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace OVS {

static bool ParseFrameRanges(const std::string& frameRangesStr, std::vector<FrameRange>& out)
{
    std::vector<std::string> rangesStr;

    size_t findPos = 0;
    while (true) {
        auto pos = frameRangesStr.find(',', findPos);
        auto count = pos - findPos;
        if (pos == std::string::npos) {
            count = pos - frameRangesStr.size();
        }

        auto sub = frameRangesStr.substr(findPos, count);
        std::erase(sub, ' ');
        rangesStr.push_back(sub);

        if (pos == std::string::npos) {
            break;
        }

        findPos = pos + 1;
    }

    std::vector<FrameRange> frameRanges;
    for (const auto& rangeStr : rangesStr) {
        auto dash = rangeStr.find('-');
        if (dash == std::string::npos) {
            uint32_t frame = 0;

            auto begin = rangeStr.data();
            auto end = begin + rangeStr.size();
            auto res = std::from_chars(begin, end, frame);
            if (res.ec != std::errc()) {
                return false;
            }

            frameRanges.push_back(FrameRange(frame, frame));
        }
        else {
            if (dash == rangeStr.size() - 1) {
                return false;
            }

            uint32_t start = 0;
            uint32_t end = 0;

            auto begin1 = rangeStr.data();
            auto end1 = begin1 + dash;
            auto begin2 = begin1 + dash + 1;
            auto end2 = begin1 + rangeStr.size();
            auto res1 = std::from_chars(begin1, end1, start);
            auto res2 = std::from_chars(begin2, end2, end);
            if (res1.ec != std::errc() || res2.ec != std::errc()) {
                return false;
            }

            frameRanges.push_back(FrameRange(start, end));
        }
    }

    out = std::move(frameRanges);
    return true;
}

VulkanLayerScreenshotSettings VulkanLayerScreenshot::ParseSettingsFromJSON(const nlohmann::json& layerInfo)
{
    VulkanLayerScreenshotSettings settings{};
    if (layerInfo.contains("Settings")) {
        const auto& settingsJSON = layerInfo["Settings"];
        if (settingsJSON.contains("FileBaseName")) {
            settings.fileBaseName = settingsJSON["FileBaseName"];
        }
        if (settingsJSON.contains("FrameRanges")) {
            std::string frameRangesStr = settingsJSON["FrameRanges"];
            if (!ParseFrameRanges(frameRangesStr, settings.frameRanges)) {
                std::cout << "[DEBUG] Could not parse Screenshot Layer frame ranges\n";
            }
        }
    }
    return settings;
}

VkResult VulkanLayerScreenshot::vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    VkResult res = next_->vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (res == VK_SUCCESS && pDevice && *pDevice) {
        VkDevice device = *pDevice;
        deviceInfos_[device].physicalDevice = physicalDevice;
    }
    return res;
}

void VulkanLayerScreenshot::vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    auto it = deviceInfos_.find(device);
    if (it != deviceInfos_.end()) {
        const auto& deviceInfo = it->second;
        for (const auto& [queue, _] : deviceInfo.queueInfos) {
            queueToDevice_.erase(queue);
        }
        deviceInfos_.erase(it);
    }
    next_->vkDestroyDevice(device, pAllocator);
}

void VulkanLayerScreenshot::vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue)
{
    next_->vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    if (!pQueue || !*pQueue) {
        return;
    }

    auto it = deviceInfos_.find(device);
    if (it == deviceInfos_.end()) {
        return;
    }

    VkQueue queue = *pQueue;
    queueToDevice_[queue] = device;

    auto& deviceInfo = it->second;
    deviceInfo.queueInfos[queue].queueFamilyIndex = queueFamilyIndex;
    deviceInfo.queueInfos[queue].queueIndex = queueIndex;
}

void VulkanLayerScreenshot::vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue)
{
    next_->vkGetDeviceQueue2(device, pQueueInfo, pQueue);
    if (!pQueueInfo || !pQueue || !*pQueue) {
        return;
    }

    auto it = deviceInfos_.find(device);
    if (it == deviceInfos_.end()) {
        return;
    }

    VkQueue queue = *pQueue;
    queueToDevice_[queue] = device;

    auto& deviceInfo = it->second;
    deviceInfo.queueInfos[queue].queueFamilyIndex = pQueueInfo->queueFamilyIndex;
    deviceInfo.queueInfos[queue].queueIndex = pQueueInfo->queueIndex;
}

VkResult VulkanLayerScreenshot::vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
{
    if (!pCreateInfo || !pSwapchain) {
        return next_->vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    }

    VkSwapchainKHR swapchain;
    VkSwapchainCreateInfoKHR modifiedInfo = *pCreateInfo;
    modifiedInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkResult res = next_->vkCreateSwapchainKHR(device, &modifiedInfo, pAllocator, &swapchain);
    if (res != VK_SUCCESS) {
        return res;
    }

    SwapchainInfo swapchainInfo{};
    swapchainInfo.width = modifiedInfo.imageExtent.width;
    swapchainInfo.height = modifiedInfo.imageExtent.height;

    uint32_t imagesCount = 0;
    res = next_->vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, nullptr);
    if (res != VK_SUCCESS) {
        return res;
    }

    swapchainInfo.images.resize(imagesCount);
    res = next_->vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, swapchainInfo.images.data());
    if (res != VK_SUCCESS) {
        return res;
    }

    *pSwapchain = swapchain;
    swapchainInfos_[swapchain] = swapchainInfo;

    return VK_SUCCESS;
}

void VulkanLayerScreenshot::vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator)
{
    auto it = swapchainInfos_.find(swapchain);
    if (it != swapchainInfos_.end()) {
        auto& swapchainInfo = it->second;
        CleanupScreenshotInfo(swapchainInfo.screenshotInfo, device);
        swapchainInfos_.erase(it);
    }
    next_->vkDestroySwapchainKHR(device, swapchain, pAllocator);
}

VkResult VulkanLayerScreenshot::vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    if (!ShouldMakeScreenshot(currentFrame_)) {
        return vkQueuePresentKHRInternal(queue, pPresentInfo);
    }

    if (!pPresentInfo || !pPresentInfo->swapchainCount) {
        return vkQueuePresentKHRInternal(queue, pPresentInfo);
    }

    auto queueIt = queueToDevice_.find(queue);
    if (queueIt == queueToDevice_.end()) {
        return vkQueuePresentKHRInternal(queue, pPresentInfo);
    }

    VkDevice device = queueIt->second;
    VkResult res = VK_SUCCESS;

    VkFence fence = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSwapchainKHR> validSwapchains;

    for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) {
        VkSwapchainKHR swapchain = pPresentInfo->pSwapchains[i];
        uint32_t imageIndex = pPresentInfo->pImageIndices[i];

        auto it = swapchainInfos_.find(swapchain);
        if (it == swapchainInfos_.end()) {
            continue;
        }

        auto& swapchainInfo = it->second;
        VkImage image = swapchainInfo.images[imageIndex];
        auto& screenshotInfo = swapchainInfo.screenshotInfo;
        if (!screenshotInfo.IsValid()) {
            res = InitScreenshotInfo(screenshotInfo, device, queue, swapchainInfo.width, swapchainInfo.height);
            if (res != VK_SUCCESS) {
                CleanupScreenshotInfo(screenshotInfo, device);
                continue;
            }
        }

        VkCommandBufferBeginInfo cbbi{};
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        res = next_->vkBeginCommandBuffer(screenshotInfo.cmdBuf, &cbbi);
        if (res != VK_SUCCESS) {
            continue;
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkImageMemoryBarrier srcPreBarrier = barrier;
        srcPreBarrier.srcAccessMask = VK_ACCESS_NONE;
        srcPreBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcPreBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        srcPreBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        srcPreBarrier.image = image;

        VkImageMemoryBarrier dstPreBarrier = barrier;
        dstPreBarrier.srcAccessMask = VK_ACCESS_NONE;
        dstPreBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstPreBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        dstPreBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstPreBarrier.image = screenshotInfo.image;

        VkImageMemoryBarrier srcPostBarrier = barrier;
        srcPostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcPostBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        srcPostBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        srcPostBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        srcPostBarrier.image = image;

        VkImageMemoryBarrier dstPostBarrier = barrier;
        dstPostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstPostBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dstPostBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstPostBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        dstPostBarrier.image = screenshotInfo.image;

        VkImageBlit region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.mipLevel = 0;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[0].x = 0;
        region.srcOffsets[0].y = 0;
        region.srcOffsets[0].z = 0;
        region.srcOffsets[1].x = swapchainInfo.width;
        region.srcOffsets[1].y = swapchainInfo.height;
        region.srcOffsets[1].z = 1;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.mipLevel = 0;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[0].x = 0;
        region.dstOffsets[0].y = 0;
        region.dstOffsets[0].z = 0;
        region.dstOffsets[1].x = swapchainInfo.width;
        region.dstOffsets[1].y = swapchainInfo.height;
        region.dstOffsets[1].z = 1;

        next_->vkCmdPipelineBarrier(screenshotInfo.cmdBuf,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    0,
                                    0,
                                    nullptr,
                                    0,
                                    nullptr,
                                    1,
                                    &srcPreBarrier);

        next_->vkCmdPipelineBarrier(screenshotInfo.cmdBuf,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    0,
                                    0,
                                    nullptr,
                                    0,
                                    nullptr,
                                    1,
                                    &dstPreBarrier);

        next_->vkCmdBlitImage(screenshotInfo.cmdBuf,
                              image,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              screenshotInfo.image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              1,
                              &region,
                              VK_FILTER_NEAREST);

        next_->vkCmdPipelineBarrier(screenshotInfo.cmdBuf,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    0,
                                    0,
                                    nullptr,
                                    0,
                                    nullptr,
                                    1,
                                    &srcPostBarrier);

        next_->vkCmdPipelineBarrier(screenshotInfo.cmdBuf,
                                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    0,
                                    0,
                                    nullptr,
                                    0,
                                    nullptr,
                                    1,
                                    &dstPostBarrier);

        res = next_->vkEndCommandBuffer(screenshotInfo.cmdBuf);
        if (res != VK_SUCCESS) {
            continue;
        }

        fence = screenshotInfo.fence;
        commandBuffers.push_back(screenshotInfo.cmdBuf);
        validSwapchains.push_back(swapchain);
    }

    if (commandBuffers.empty()) {
        return vkQueuePresentKHRInternal(queue, pPresentInfo);
    }

    std::vector<VkPipelineStageFlags> waitDstStageMask(pPresentInfo->waitSemaphoreCount, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = pPresentInfo->waitSemaphoreCount;
    si.pWaitSemaphores = pPresentInfo->pWaitSemaphores;
    si.pWaitDstStageMask = waitDstStageMask.data();
    si.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    si.pCommandBuffers = commandBuffers.data();

    res = next_->vkQueueSubmit(queue, 1, &si, fence);
    if (res != VK_SUCCESS) {
        return res;
    }

    res = next_->vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    if (res != VK_SUCCESS) {
        return res;
    }

    res = next_->vkResetFences(device, 1, &fence);
    if (res != VK_SUCCESS) {
        return res;
    }

    for (auto swapchain : validSwapchains) {
        auto& swapchainInfo = swapchainInfos_[swapchain];
        auto& screenshotInfo = swapchainInfo.screenshotInfo;

        res = next_->vkResetCommandPool(device, screenshotInfo.cmdPool, 0);
        if (res != VK_SUCCESS) {
            continue;
        }

        void* pData = nullptr;
        res = next_->vkMapMemory(device, screenshotInfo.memory, 0, VK_WHOLE_SIZE, 0, &pData);
        if (res != VK_SUCCESS) {
            continue;
        }

        pData = (void*)((uint8_t*)pData + screenshotInfo.offset);

        const auto& fileBaseName = settings_.fileBaseName;
        std::string filename = fileBaseName + '-' + std::to_string(currentFrame_) + ".png";
        int pngRes = stbi_write_png(filename.c_str(),
                                    swapchainInfo.width,
                                    swapchainInfo.height,
                                    4,
                                    pData,
                                    screenshotInfo.stride);
        
        next_->vkUnmapMemory(device, screenshotInfo.memory);
    }

    VkPresentInfoKHR modifiedInfo = *pPresentInfo;
    modifiedInfo.waitSemaphoreCount = 0;
    modifiedInfo.pWaitSemaphores = nullptr;
    return vkQueuePresentKHRInternal(queue, &modifiedInfo);
}

VkResult VulkanLayerScreenshot::InitScreenshotInfo(ScreenshotInfo& screenshotInfo, VkDevice device, VkQueue queue, uint32_t width, uint32_t height)
{
    auto deviceInfoIt = deviceInfos_.find(device);
    if (deviceInfoIt == deviceInfos_.end()) {
        return VK_ERROR_DEVICE_LOST;
    }

    const auto& deviceInfo = deviceInfoIt->second;
    const auto& physicalDevice = deviceInfo.physicalDevice;
    const auto& queueInfos = deviceInfo.queueInfos;

    auto queueInfoIt = queueInfos.find(queue);
    if (queueInfoIt == queueInfos.end()) {
        return VK_ERROR_DEVICE_LOST;
    }

    const auto& queueInfo = queueInfoIt->second;

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent.width = width;
    ici.extent.height = height;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_LINEAR;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.queueFamilyIndexCount = 0;
    ici.pQueueFamilyIndices = nullptr;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult res = next_->vkCreateImage(device, &ici, nullptr, &screenshotInfo.image);
    if (res != VK_SUCCESS) {
        return res;
    }

    VkMemoryRequirements memReqs{};
    next_->vkGetImageMemoryRequirements(device, screenshotInfo.image, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps{};
    next_->vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    int32_t memoryTypeIndex = -1;
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReqs.memoryTypeBits & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            memoryTypeIndex = i;
            break;
        }
    }

    if (memoryTypeIndex == -1) {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReqs.size;
    mai.memoryTypeIndex = memoryTypeIndex;

    res = next_->vkAllocateMemory(device, &mai, nullptr, &screenshotInfo.memory);
    if (res != VK_SUCCESS) {
        return res;
    }

    res = next_->vkBindImageMemory(device, screenshotInfo.image, screenshotInfo.memory, 0);
    if (res != VK_SUCCESS) {
        return res;
    }

    VkImageSubresource subResource{};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subResource.mipLevel = 0;
    subResource.arrayLayer = 0;

    VkSubresourceLayout subResourceLayout{};
    next_->vkGetImageSubresourceLayout(device, screenshotInfo.image, &subResource, &subResourceLayout);

    screenshotInfo.offset = subResourceLayout.offset;
    screenshotInfo.stride = subResourceLayout.rowPitch;

    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = queueInfo.queueFamilyIndex;

    res = next_->vkCreateCommandPool(device, &cpci, nullptr, &screenshotInfo.cmdPool);
    if (res != VK_SUCCESS) {
        return res;
    }

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = screenshotInfo.cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    res = next_->vkAllocateCommandBuffers(device, &cbai, &screenshotInfo.cmdBuf);
    if (res != VK_SUCCESS) {
        return res;
    }

    PatchDispatchKey(device, screenshotInfo.cmdBuf);

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    res = next_->vkCreateFence(device, &fci, nullptr, &screenshotInfo.fence);
    if (res != VK_SUCCESS) {
        return res;
    }

    VkCommandBufferBeginInfo cbbi{};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    res = next_->vkBeginCommandBuffer(screenshotInfo.cmdBuf, &cbbi);
    if (res != VK_SUCCESS) {
        return res;
    }

    VkImageMemoryBarrier imb{};
    imb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imb.srcAccessMask = 0;
    imb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imb.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imb.image = screenshotInfo.image;
    imb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imb.subresourceRange.baseMipLevel = 0;
    imb.subresourceRange.levelCount = 1;
    imb.subresourceRange.baseArrayLayer = 0;
    imb.subresourceRange.layerCount = 1;

    next_->vkCmdPipelineBarrier(screenshotInfo.cmdBuf,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                1,
                                &imb);

    res = next_->vkEndCommandBuffer(screenshotInfo.cmdBuf);
    if (res != VK_SUCCESS) {
        return res;
    }

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &screenshotInfo.cmdBuf;

    res = next_->vkQueueSubmit(queue, 1, &si, screenshotInfo.fence);
    if (res != VK_SUCCESS) {
        return res;
    }

    res = next_->vkWaitForFences(device, 1, &screenshotInfo.fence, VK_TRUE, UINT64_MAX);
    if (res != VK_SUCCESS) {
        return res;
    }

    res = next_->vkResetFences(device, 1, &screenshotInfo.fence);
    if (res != VK_SUCCESS) {
        return res;
    }

    res = next_->vkResetCommandPool(device, screenshotInfo.cmdPool, 0);
    if (res != VK_SUCCESS) {
        return res;
    }

    return VK_SUCCESS;
}

void VulkanLayerScreenshot::CleanupScreenshotInfo(ScreenshotInfo& screenshotInfo, VkDevice device)
{
    if (screenshotInfo.image) {
        next_->vkDestroyImage(device, screenshotInfo.image, nullptr);
    }

    if (screenshotInfo.memory) {
        next_->vkFreeMemory(device, screenshotInfo.memory, nullptr);
    }

    if (screenshotInfo.cmdPool) {
        next_->vkDestroyCommandPool(device, screenshotInfo.cmdPool, nullptr);
    }

    if (screenshotInfo.fence) {
        next_->vkDestroyFence(device, screenshotInfo.fence, nullptr);
    }

    screenshotInfo = ScreenshotInfo();
}

bool VulkanLayerScreenshot::ShouldMakeScreenshot(uint32_t frame) const
{
    const auto& frameRanges = settings_.frameRanges;
    for (const auto& fr : frameRanges) {
        if (fr.start <= frame && frame <= fr.end) {
            return true;
        }
    }
    return false;
}

} // namespace OVS