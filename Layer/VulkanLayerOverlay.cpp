#include <VulkanUtils.h>
#include <VulkanLayerOverlay.h>

namespace OVS {

VkResult VulkanLayerOverlay::vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    VkResult res = next_->vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (res == VK_SUCCESS && pDevice && *pDevice) {
        VkDevice device = *pDevice;
        auto& deviceInfo = deviceInfos_[device];

        deviceInfo.physicalDevice = physicalDevice;

        uint32_t queueFamilyCount = 0;
        next_->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        next_->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

        std::optional<uint32_t> graphicsIndex;
        std::optional<uint32_t> computeIndex;
        std::optional<uint32_t> pureGraphicsIndex;
        std::optional<uint32_t> pureComputeIndex;

        for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; ++i) {
            const auto& queueCreateInfo = pCreateInfo->pQueueCreateInfos[i];
            if (queueCreateInfo.queueCount == 0) {
                continue;
            }

            uint32_t queueFamilyIndex = queueCreateInfo.queueFamilyIndex;
            const auto& familyProperties = queueFamilyProperties[queueFamilyIndex];
            VkQueueFlags queueFlags = familyProperties.queueFlags;

            if (queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if (queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)) {
                    graphicsIndex = queueFamilyIndex;
                }
                else {
                    pureGraphicsIndex = queueFamilyIndex;
                }
            }

            if (queueFlags & VK_QUEUE_COMPUTE_BIT) {
                if (queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT)) {
                    computeIndex = queueFamilyIndex;
                }
                else {
                    pureComputeIndex = queueFamilyIndex;
                }
            }
        }

        const auto& graphicsQueueFamilyIndex = (pureGraphicsIndex) ? (pureGraphicsIndex) : (graphicsIndex);
        const auto& computeQueueFamilyIndex = (pureComputeIndex) ? (pureComputeIndex) : (computeIndex);

        if (graphicsQueueFamilyIndex) {
            deviceInfo.graphicsQueueFamilyIndex = graphicsQueueFamilyIndex.value();
            next_->vkGetDeviceQueue(device, graphicsQueueFamilyIndex.value(), 0, &deviceInfo.graphicsQueue);
        }
        if (computeQueueFamilyIndex) {
            deviceInfo.computeQueueFamilyIndex = computeQueueFamilyIndex.value();
            next_->vkGetDeviceQueue(device, computeQueueFamilyIndex.value(), 0, &deviceInfo.computeQueue);
        }
    }
    return res;
}

void VulkanLayerOverlay::vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    deviceInfos_.erase(device);
    next_->vkDestroyDevice(device, pAllocator);
}

VkResult VulkanLayerOverlay::vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
{
    if (!pCreateInfo || !pSwapchain) {
        return next_->vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    }

    VkSwapchainKHR swapchain;
    VkSwapchainCreateInfoKHR modifiedInfo = *pCreateInfo;
    modifiedInfo.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

    VkResult res = next_->vkCreateSwapchainKHR(device, &modifiedInfo, pAllocator, &swapchain);
    if (res != VK_SUCCESS) {
        return res;
    }

    SwapchainInfo swapchainInfo{};
    swapchainInfo.width = modifiedInfo.imageExtent.width;
    swapchainInfo.height = modifiedInfo.imageExtent.height;
    swapchainInfo.format = modifiedInfo.imageFormat;

    uint32_t imagesCount = 0;
    res = next_->vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, nullptr);
    if (res != VK_SUCCESS) {
        next_->vkDestroySwapchainKHR(device, swapchain, pAllocator);
        return res;
    }

    swapchainInfo.images.resize(imagesCount);
    res = next_->vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, swapchainInfo.images.data());
    if (res != VK_SUCCESS) {
        next_->vkDestroySwapchainKHR(device, swapchain, pAllocator);
        return res;
    }

    OverlayInfo overlayInfo{};
    res = CreateOverlayInfo(device, pAllocator, swapchainInfo, overlayInfo);
    if (res != VK_SUCCESS) {
        next_->vkDestroySwapchainKHR(device, swapchain, pAllocator);
        return res;
    }

    *pSwapchain = swapchain;
    swapchainInfos_[swapchain] = std::move(swapchainInfo);
    overlayInfos_[swapchain] = std::move(overlayInfo);

    return VK_SUCCESS;
}

void VulkanLayerOverlay::vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator)
{
    auto it = overlayInfos_.find(swapchain);
    if (it != overlayInfos_.end()) {
        const auto& overlayInfo = it->second;
        DestroyOverlayInfo(device, pAllocator, overlayInfo);
        overlayInfos_.erase(it);
    }
    swapchainInfos_.erase(swapchain);
    next_->vkDestroySwapchainKHR(device, swapchain, pAllocator);
}

VkResult VulkanLayerOverlay::vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    if (!pPresentInfo || !pPresentInfo->swapchainCount) {
        return next_->vkQueuePresentKHR(queue, pPresentInfo);
    }

    VkResult res = VK_SUCCESS;
    VkFence fence = VK_NULL_HANDLE;

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> signalSemaphores;

    for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i) {
        VkSwapchainKHR swapchain = pPresentInfo->pSwapchains[i];
        uint32_t imageIndex = pPresentInfo->pImageIndices[i];

        auto swapchainInfoIt = swapchainInfos_.find(swapchain);
        if (swapchainInfoIt == swapchainInfos_.end()) {
            continue;
        }

        auto overlayInfoIt = overlayInfos_.find(swapchain);
        if (overlayInfoIt == overlayInfos_.end()) {
            continue;
        }

        const auto& swapchainInfo = swapchainInfoIt->second;
        const auto& overlayInfo = overlayInfoIt->second;

        auto device = overlayInfo.device;
        auto cmdBuf = overlayInfo.cmdBufs[imageIndex];
        auto semaphore = overlayInfo.semaphores[imageIndex];
        auto renderPass = overlayInfo.renderPasses[imageIndex];
        auto framebuffer = overlayInfo.framebuffers[imageIndex];

        fence = overlayInfo.fences[imageIndex];

        res = next_->vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        if (res != VK_SUCCESS) {
            continue;
        }

        res = next_->vkResetFences(device, 1, &fence);
        if (res != VK_SUCCESS) {
            continue;
        }

        VkCommandBufferBeginInfo cbbi{};
        cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        res = next_->vkBeginCommandBuffer(cmdBuf, &cbbi);
        if (res != VK_SUCCESS) {
            continue;
        }

        VkRenderPassBeginInfo rpbi{};
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = renderPass;
        rpbi.framebuffer = framebuffer;
        rpbi.renderArea.extent.width = swapchainInfo.width;
        rpbi.renderArea.extent.height = swapchainInfo.height;

        next_->vkCmdBeginRenderPass(cmdBuf, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        // TODO

        next_->vkCmdEndRenderPass(cmdBuf);

        res = next_->vkEndCommandBuffer(cmdBuf);
        if (res != VK_SUCCESS) {
            continue;
        }

        commandBuffers.push_back(cmdBuf);
        signalSemaphores.push_back(semaphore);
    }

    if (commandBuffers.empty()) {
        return next_->vkQueuePresentKHR(queue, pPresentInfo);
    }

    std::vector<VkPipelineStageFlags> waitDstStageMask(pPresentInfo->waitSemaphoreCount, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = pPresentInfo->waitSemaphoreCount;
    si.pWaitSemaphores = pPresentInfo->pWaitSemaphores;
    si.pWaitDstStageMask = waitDstStageMask.data();
    si.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
    si.pCommandBuffers = commandBuffers.data();
    si.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    si.pSignalSemaphores = signalSemaphores.data();

    res = next_->vkQueueSubmit(queue, 1, &si, fence);
    if (res != VK_SUCCESS) {
        return res;
    }

    VkPresentInfoKHR modifiedInfo = *pPresentInfo;
    modifiedInfo.waitSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    modifiedInfo.pWaitSemaphores = signalSemaphores.data();
    return next_->vkQueuePresentKHR(queue, &modifiedInfo);
}

VkResult VulkanLayerOverlay::CreateOverlayInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const SwapchainInfo& swapchainInfo, OverlayInfo& overlayInfoOut)
{
    auto deviceInfoIt = deviceInfos_.find(device);
    if (deviceInfoIt == deviceInfos_.end()) {
        return VK_ERROR_DEVICE_LOST;
    }

    const auto& deviceInfo = deviceInfoIt->second;

    uint32_t swapchainImagesCount = swapchainInfo.images.size();

    OverlayInfo overlayInfo{};
    overlayInfo.device = device;
    overlayInfo.queue = deviceInfo.graphicsQueue;

    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = deviceInfo.graphicsQueueFamilyIndex;

    VkResult res = next_->vkCreateCommandPool(device, &cpci, pAllocator, &overlayInfo.cmdPool);
    if (res != VK_SUCCESS) {
        return res;
    }

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = overlayInfo.cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = swapchainImagesCount;

    overlayInfo.cmdBufs.resize(swapchainImagesCount);
    res = next_->vkAllocateCommandBuffers(device, &cbai, overlayInfo.cmdBufs.data());
    if (res != VK_SUCCESS) {
        DestroyOverlayInfo(device, pAllocator, overlayInfo);
        return res;
    }

    for (auto cmdBuf : overlayInfo.cmdBufs) {
        PatchDispatchKey(device, cmdBuf);
    }

    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkAttachmentDescription attachmentDesc{};
    attachmentDesc.format = swapchainInfo.format;
	attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference attachmentRef{};
	attachmentRef.attachment = 0;
	attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDesc{};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &attachmentRef;

	VkRenderPassCreateInfo rpci{};
	rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount = 1;
	rpci.pAttachments = &attachmentDesc;
	rpci.subpassCount = 1;
	rpci.pSubpasses = &subpassDesc;

    VkImageViewCreateInfo ivci{};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = swapchainInfo.format;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount = 1;

    VkFramebufferCreateInfo fbci{};
    fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbci.attachmentCount = 1;
    fbci.width = swapchainInfo.width;
    fbci.height = swapchainInfo.height;
    fbci.layers = 1;

    for (uint32_t i = 0; i < swapchainImagesCount; ++i) {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        res = next_->vkCreateSemaphore(device, &sci, pAllocator, &semaphore);
        if (res != VK_SUCCESS) {
            break;
        }

        overlayInfo.semaphores.push_back(semaphore);

        VkFence fence = VK_NULL_HANDLE;
        res = next_->vkCreateFence(device, &fci, pAllocator, &fence);
        if (res != VK_SUCCESS) {
            break;
        }

        overlayInfo.fences.push_back(fence);

        VkRenderPass renderPass = VK_NULL_HANDLE;
        res = next_->vkCreateRenderPass(device, &rpci, pAllocator, &renderPass);
        if (res != VK_SUCCESS) {
            break;
        }

        overlayInfo.renderPasses.push_back(renderPass);

        VkImageView imageView = VK_NULL_HANDLE;
        ivci.image = swapchainInfo.images[i];
        res = next_->vkCreateImageView(device, &ivci, pAllocator, &imageView);
        if (res != VK_SUCCESS) {
            break;
        }

        overlayInfo.imageViews.push_back(imageView);

        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        fbci.renderPass = renderPass;
        fbci.pAttachments = &imageView;
        res = next_->vkCreateFramebuffer(device, &fbci, pAllocator, &framebuffer);
        if (res != VK_SUCCESS) {
            break;
        }

        overlayInfo.framebuffers.push_back(framebuffer);
    }

    if (res != VK_SUCCESS) {
        DestroyOverlayInfo(device, pAllocator, overlayInfo);
        return res;
    }

    overlayInfoOut = std::move(overlayInfo);
    return res;
}

void VulkanLayerOverlay::DestroyOverlayInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const OverlayInfo& overlayInfo)
{
    for (auto fb : overlayInfo.framebuffers) {
        next_->vkDestroyFramebuffer(device, fb, pAllocator);
    }
    for (auto iv : overlayInfo.imageViews) {
        next_->vkDestroyImageView(device, iv, pAllocator);
    }
    for (auto rp : overlayInfo.renderPasses) {
        next_->vkDestroyRenderPass(device, rp, pAllocator);
    }
    for (auto fence : overlayInfo.fences) {
        next_->vkDestroyFence(device, fence, pAllocator);
    }
    for (auto sem : overlayInfo.semaphores) {
        next_->vkDestroySemaphore(device, sem, pAllocator);
    }
    next_->vkDestroyCommandPool(device, overlayInfo.cmdPool, pAllocator);
}

} // namespace OVS