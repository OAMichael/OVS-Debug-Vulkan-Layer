#include <VulkanUtils.h>
#include <VulkanLayerOverlay.h>
#include <VulkanLayerTerminator.h>

#include <commctrl.h>

#include <iostream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace OVS {

static void vkGetDeviceQueueForImGui(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue* pQueue)
{
    const auto& dispatchTable = VulkanLayerTerminator::GetDeviceDispatchTable(device);

    dispatchTable.vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
    if (pQueue && *pQueue) {
        auto queue = *pQueue;
        PatchDispatchKey(device, queue);
    }
}

static void vkGetDeviceQueue2ForImGui(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue)
{
    const auto& dispatchTable = VulkanLayerTerminator::GetDeviceDispatchTable(device);

    dispatchTable.vkGetDeviceQueue2(device, pQueueInfo, pQueue);
    if (pQueue && *pQueue) {
        auto queue = *pQueue;
        PatchDispatchKey(device, queue);
    }
}

static VkResult vkAllocateCommandBuffersForImGui(VkDevice device, const VkCommandBufferAllocateInfo *pAllocateInfo, VkCommandBuffer *pCommandBuffers)
{
    const auto& dispatchTable = VulkanLayerTerminator::GetDeviceDispatchTable(device);

    VkResult res = dispatchTable.vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
    if (res == VK_SUCCESS) {
        for (uint32_t i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
            auto cmdBuf = pCommandBuffers[i];
            PatchDispatchKey(device, cmdBuf);
        }
    }
    return res;
}

static PFN_vkVoidFunction VulkanLoaderFuncForImGui(const char* funcName, void* userData)
{
    if (!std::strcmp(funcName, "vkAllocateCommandBuffers")) {
        return (PFN_vkVoidFunction)vkAllocateCommandBuffersForImGui;
    }
    if (!std::strcmp(funcName, "vkGetDeviceQueue")) {
        return (PFN_vkVoidFunction)vkGetDeviceQueueForImGui;
    }
    if (!std::strcmp(funcName, "vkGetDeviceQueue2")) {
        return (PFN_vkVoidFunction)vkGetDeviceQueue2ForImGui;
    }

    auto instance = *reinterpret_cast<VkInstance*>(userData);
    const auto& dispatchTable = VulkanLayerTerminator::GetInstanceDispatchTable(instance);
    return dispatchTable.vkGetInstanceProcAddr(instance, funcName);
}

static void VulkanResultFuncForImGui(VkResult res)
{
    if (res < VK_SUCCESS) {
        std::cout << "ImGui Vulkan Error: " << res << '\n';
    }
}

static int VulkanCreateSurfaceForImGui(ImGuiViewport* viewport, ImU64 instance, const void* pAllocator, ImU64* pSurface)
{
    const auto& dispatchTable = VulkanLayerTerminator::GetInstanceDispatchTable((VkInstance)instance);

    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = (HWND)viewport->PlatformHandleRaw;
    createInfo.hinstance = ::GetModuleHandle(nullptr);
    return (int)dispatchTable.vkCreateWin32SurfaceKHR((VkInstance)instance, &createInfo, (VkAllocationCallbacks*)pAllocator, (VkSurfaceKHR*)pSurface);
}

VulkanLayerOverlaySettings VulkanLayerOverlay::ParseSettingsFromJSON(const nlohmann::json& layerInfo)
{
    VulkanLayerOverlaySettings settings{};
    if (layerInfo.contains("Settings")) {
        const auto& settingsJSON = layerInfo["Settings"];
        if (settingsJSON.contains("MultipleViewports")) {
            settings.multipleViewports = settingsJSON["MultipleViewports"];
        }
    }
    return settings;
}

VulkanLayerOverlay::VulkanLayerOverlay(const VulkanLayerOverlaySettings& settings) : VulkanLayerPassThrough(VulkanLayerType::Overlay), settings_{settings}
{
    ImGui_ImplWin32_EnableDpiAwareness();
    HMONITOR mainMonitor = ::MonitorFromPoint(POINT(0, 0), MONITOR_DEFAULTTOPRIMARY);
    float mainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(mainMonitor);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (settings_.multipleViewports) {
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(mainScale);
    style.FontScaleDpi = mainScale;
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;
    if (settings_.multipleViewports) {
        io.ConfigViewportsNoAutoMerge = true;
        io.ConfigViewportsNoDefaultParent = false;
    }
}

VulkanLayerOverlay::~VulkanLayerOverlay()
{
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }
}

VkResult VulkanLayerOverlay::vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
    VkResult res = next_->vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (res == VK_SUCCESS) {
        VkInstance instance = *pInstance;

        auto& instanceInfo = instanceInfos_[instance];
        instanceInfo.apiVersion = pCreateInfo->pApplicationInfo->apiVersion;
    }
    return res;
}

void VulkanLayerOverlay::vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
    instanceInfos_.erase(instance);
    next_->vkDestroyInstance(instance, pAllocator);
}

VkResult VulkanLayerOverlay::vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices)
{
    VkResult res = next_->vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    if ((res == VK_SUCCESS || res == VK_INCOMPLETE) && pPhysicalDeviceCount && pPhysicalDevices) {
        uint32_t physicalDeviceCount = *pPhysicalDeviceCount;
        for (uint32_t i = 0; i < physicalDeviceCount; ++i) {
            VkPhysicalDevice physicalDevice = pPhysicalDevices[i];

            auto& physicalDeviceInfo = physicalDeviceInfos_[physicalDevice];
            physicalDeviceInfo.instance = instance;
        }
    }
    return res;
}

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

VkResult VulkanLayerOverlay::vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
    VkResult res = next_->vkCreateWin32SurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
    if (res == VK_SUCCESS && pSurface && *pSurface) {
        VkSurfaceKHR surface = *pSurface;
        auto& surfaceInfo = surfaceInfos_[surface];

        surfaceInfo.type = SurfaceType::Windows;
        surfaceInfo.window = pCreateInfo->hwnd;
        ReplaceWndProc(pCreateInfo->hwnd);
    }
    return res;
}

void VulkanLayerOverlay::vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator)
{
    surfaceInfos_.erase(surface);
    next_->vkDestroySurfaceKHR(instance, surface, pAllocator);
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
    swapchainInfo.device = device;
    swapchainInfo.surface = modifiedInfo.surface;
    swapchainInfo.width = modifiedInfo.imageExtent.width;
    swapchainInfo.height = modifiedInfo.imageExtent.height;
    swapchainInfo.format = modifiedInfo.imageFormat;
    swapchainInfo.imageColorSpace = modifiedInfo.imageColorSpace;
    swapchainInfo.presentMode = modifiedInfo.presentMode;

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

    auto imguiInfoIt = imguiInfos_.find(modifiedInfo.oldSwapchain);
    if (imguiInfoIt != imguiInfos_.end()) {
        const auto& imguiInfo = imguiInfoIt->second;
        DestroyImGuiInfo(imguiInfo);
        imguiInfos_.erase(imguiInfoIt);
    }

    ImGuiInfo imguiInfo{};
    res = CreateImGuiInfo(swapchain, swapchainInfo, overlayInfo, imguiInfo);
    if (res != VK_SUCCESS) {
        DestroyOverlayInfo(device, pAllocator, overlayInfo);
        next_->vkDestroySwapchainKHR(device, swapchain, pAllocator);
        return res;
    }

    *pSwapchain = swapchain;
    swapchainInfos_[swapchain] = std::move(swapchainInfo);
    overlayInfos_[swapchain] = std::move(overlayInfo);
    imguiInfos_[swapchain] = std::move(imguiInfo);

    return VK_SUCCESS;
}

void VulkanLayerOverlay::vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator)
{
    auto imguiInfoIt = imguiInfos_.find(swapchain);
    if (imguiInfoIt != imguiInfos_.end()) {
        const auto& imguiInfo = imguiInfoIt->second;
        DestroyImGuiInfo(imguiInfo);
        imguiInfos_.erase(imguiInfoIt);
    }

    auto overlayInfoIt = overlayInfos_.find(swapchain);
    if (overlayInfoIt != overlayInfos_.end()) {
        const auto& overlayInfo = overlayInfoIt->second;
        DestroyOverlayInfo(device, pAllocator, overlayInfo);
        overlayInfos_.erase(overlayInfoIt);
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
        auto framebuffer = overlayInfo.framebuffers[imageIndex];

        fence = overlayInfo.fences[imageIndex];
        auto renderPass = overlayInfo.renderPass;

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

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();

        bool mainWindowMinimized = (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);
        if (!mainWindowMinimized) {
            ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuf);
        }

        next_->vkCmdEndRenderPass(cmdBuf);

        res = next_->vkEndCommandBuffer(cmdBuf);
        if (res != VK_SUCCESS) {
            continue;
        }

        if (settings_.multipleViewports) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
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

    VkRenderPass renderPass = VK_NULL_HANDLE;
    res = next_->vkCreateRenderPass(device, &rpci, pAllocator, &renderPass);
    if (res != VK_SUCCESS) {
        DestroyOverlayInfo(device, pAllocator, overlayInfo);
        return res;
    }

    overlayInfo.renderPass = renderPass;

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

    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                   OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,             OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,      OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,      OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,    OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,    OverlayInfoMaxDescriptorsCount },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,          OverlayInfoMaxDescriptorsCount }
    };

    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.maxSets = OverlayInfoMaxDescriptorSetsCount;
    dpci.poolSizeCount = std::size(poolSizes);
    dpci.pPoolSizes = poolSizes;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    res = next_->vkCreateDescriptorPool(device, &dpci, pAllocator, &descriptorPool);
    if (res != VK_SUCCESS) {
        DestroyOverlayInfo(device, pAllocator, overlayInfo);
        return res;
    }

    overlayInfo.descriptorPool = descriptorPool;

    overlayInfoOut = std::move(overlayInfo);
    return res;
}

void VulkanLayerOverlay::DestroyOverlayInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const OverlayInfo& overlayInfo)
{
    next_->vkDestroyDescriptorPool(device, overlayInfo.descriptorPool, pAllocator);

    for (auto fb : overlayInfo.framebuffers) {
        next_->vkDestroyFramebuffer(device, fb, pAllocator);
    }
    for (auto iv : overlayInfo.imageViews) {
        next_->vkDestroyImageView(device, iv, pAllocator);
    }
    next_->vkDestroyRenderPass(device, overlayInfo.renderPass, pAllocator);

    for (auto fence : overlayInfo.fences) {
        next_->vkDestroyFence(device, fence, pAllocator);
    }
    for (auto sem : overlayInfo.semaphores) {
        next_->vkDestroySemaphore(device, sem, pAllocator);
    }
    next_->vkDestroyCommandPool(device, overlayInfo.cmdPool, pAllocator);
}

VkResult VulkanLayerOverlay::CreateImGuiInfo(VkSwapchainKHR swapchain, const SwapchainInfo& swapchainInfo, const OverlayInfo& overlayInfo, ImGuiInfo& imguiInfoOut)
{
    auto surfaceInfoIt = surfaceInfos_.find(swapchainInfo.surface);
    if (surfaceInfoIt == surfaceInfos_.end()) {
        return VK_ERROR_UNKNOWN;
    }

    const auto& surfaceInfo = surfaceInfoIt->second;

    auto deviceInfoIt = deviceInfos_.find(swapchainInfo.device);
    if (deviceInfoIt == deviceInfos_.end()) {
        return VK_ERROR_UNKNOWN;
    }

    const auto& deviceInfo = deviceInfoIt->second;

    auto physicalDeviceInfoIt = physicalDeviceInfos_.find(deviceInfo.physicalDevice);
    if (physicalDeviceInfoIt == physicalDeviceInfos_.end()) {
        return VK_ERROR_UNKNOWN;
    }

    const auto& physicalDeviceInfo = physicalDeviceInfoIt->second;
    VkInstance instance = physicalDeviceInfo.instance;

    auto instanceInfoIt = instanceInfos_.find(instance);
    if (instanceInfoIt == instanceInfos_.end()) {
        return VK_ERROR_UNKNOWN;
    }

    const auto& instanceInfo = instanceInfoIt->second;

    ImGuiInfo imguiInfo{};
    imguiInfo.instance = instance;
    imguiInfo.physicalDevice = deviceInfo.physicalDevice;
    imguiInfo.device = swapchainInfo.device;
    imguiInfo.queueFamily = deviceInfo.graphicsQueueFamilyIndex;
    imguiInfo.queue = deviceInfo.graphicsQueue;

    if (!ImGui_ImplWin32_Init(surfaceInfo.window)) {
        return VK_ERROR_UNKNOWN;
    }

    ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
    platformIO.Platform_CreateVkSurface = VulkanCreateSurfaceForImGui;

    if (!ImGui_ImplVulkan_LoadFunctions(instanceInfo.apiVersion, &VulkanLoaderFuncForImGui, &instance)) {
        ImGui_ImplWin32_Shutdown();
        return VK_ERROR_UNKNOWN;
    }

    uint32_t swapchainImageCount = swapchainInfo.images.size();

    auto& windowData = imguiInfo.windowData;

    windowData.Surface = swapchainInfo.surface;
    windowData.SurfaceFormat.format = swapchainInfo.format;
    windowData.SurfaceFormat.colorSpace = swapchainInfo.imageColorSpace;
    windowData.PresentMode = swapchainInfo.presentMode;

    windowData.AttachmentDesc.format = swapchainInfo.format;
	windowData.AttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	windowData.AttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	windowData.AttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	windowData.AttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	windowData.AttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	windowData.AttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	windowData.AttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    windowData.Width = swapchainInfo.width;
    windowData.Height = swapchainInfo.height;
    windowData.Swapchain = swapchain;
    windowData.RenderPass = overlayInfo.renderPass;
    windowData.ImageCount = swapchainImageCount;
    windowData.SemaphoreCount = 0;

    windowData.Frames.resize(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        windowData.Frames[i].CommandPool = overlayInfo.cmdPool;
        windowData.Frames[i].CommandBuffer = overlayInfo.cmdBufs[i];
        windowData.Frames[i].Fence = overlayInfo.fences[i];
        windowData.Frames[i].Backbuffer = swapchainInfo.images[i];
        windowData.Frames[i].BackbufferView = overlayInfo.imageViews[i];
        windowData.Frames[i].Framebuffer = overlayInfo.framebuffers[i];
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = instanceInfo.apiVersion;
    initInfo.Instance = imguiInfo.instance;
    initInfo.PhysicalDevice = imguiInfo.physicalDevice;
    initInfo.Device = imguiInfo.device;
    initInfo.QueueFamily = imguiInfo.queueFamily;
    initInfo.Queue = imguiInfo.queue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = overlayInfo.descriptorPool;
    initInfo.MinImageCount = windowData.ImageCount;
    initInfo.ImageCount = windowData.ImageCount;
    initInfo.Allocator = nullptr;
    initInfo.PipelineInfoMain.RenderPass = windowData.RenderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn = &VulkanResultFuncForImGui;
    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        ImGui_ImplWin32_Shutdown();
        return VK_ERROR_UNKNOWN;
    }

    imguiInfoOut = imguiInfo;
    return VK_SUCCESS;
}

void VulkanLayerOverlay::DestroyImGuiInfo(const ImGuiInfo& imguiInfo)
{
    VkDevice device = imguiInfo.device;
    const auto& dtNative = VulkanLayerTerminator::GetDeviceDispatchTable(device);

    dtNative.vkDeviceWaitIdle(device);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
}

static bool IsMouseMessageForImGui(UINT uMsg)
{
    switch (uMsg) {
        case WM_MOUSEMOVE:
        case WM_NCMOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_NCMOUSELEAVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_SETCURSOR: {
            return true;
        }
        default: {
            return false;
        }
    }
}

static bool IsKeyboardMessageForImGui(UINT uMsg)
{
    switch (uMsg) {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_INPUTLANGCHANGE:
        case WM_CHAR:
        case WM_IME_COMPOSITION:
        case WM_IME_CHAR: {
            return true;
        }
        default: {
            return false;
        }
    }
}

LRESULT CALLBACK SubclassWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) {
        return true;
    }

    const VulkanLayerOverlay* layer = reinterpret_cast<const VulkanLayerOverlay*>(dwRefData);
    const auto& settings = layer->GetSettings();
    if (!settings.multipleViewports) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse && IsMouseMessageForImGui(uMsg)) {
            return true;
        }
        if (io.WantCaptureKeyboard && IsKeyboardMessageForImGui(uMsg)) {
            return true;
        }
    }

    if (uMsg == WM_NCDESTROY) {
        RemoveWindowSubclass(hWnd, SubclassWindowProc, uIdSubclass);
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void VulkanLayerOverlay::ReplaceWndProc(HWND hwnd) {
    SetWindowSubclass(hwnd, SubclassWindowProc, 1, (DWORD_PTR)this);
}

} // namespace OVS