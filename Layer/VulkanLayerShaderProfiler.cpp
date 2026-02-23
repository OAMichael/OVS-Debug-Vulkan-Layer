#include <VulkanLayerShaderProfiler.h>
#include <VulkanUtils.h>

#include <StructDeepCopyGenerated.h>

#include <spirv-tools/libspirv.hpp>
#include <opt/build_module.h>
#include <spirv_constant.h>

#include <iostream>
#include <fstream>
#include <string>

namespace OVS {

static void SPIRVErrorHandler(spv_message_level_t, const char*, const spv_position_t&, const char* m) {
    std::cout << "SPIRV: " << m << '\n';
};

static void DumpModule(const spvtools::opt::Module& m) {
    std::cout << "Module (version: " << m.version() << "):\n";
    for (const auto& inst : m.capabilities()) {
        std::cout << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.ext_inst_imports()) {
        std::cout << "            " << inst.PrettyPrint() << '\n';
    }
    std::cout << "            " << m.GetMemoryModel()->PrettyPrint() << '\n';
    for (const auto& inst : m.entry_points()) {
        std::cout << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.execution_modes()) {
        std::cout << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.annotations()) {
        std::cout << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.extensions()) {
        std::cout << "            " << inst.PrettyPrint() << '\n';
    }
    for (const auto& inst : m.types_values()) {
        std::cout << "            " << inst.PrettyPrint() << '\n';
    }
    std::cout << '\n';

    for (const auto& f : m) {
        std::cout << "    Function #" << f.result_id() << '\n';
        for (const auto& bb : f) {
            std::cout << "        BB #" << bb.id() << '\n';
            for (const auto& inst : bb) {
                std::cout << "            " << inst.PrettyPrint() << '\n';
            }
            std::cout << '\n';
        }
        std::cout << '\n';
    }
    std::cout << '\n';
}

static VulkanShaderStage ExecutionModelToShaderStage(spv::ExecutionModel value) {
    switch (value) {
        case spv::ExecutionModel::Vertex:                   return VulkanShaderStage::Vertex;
        case spv::ExecutionModel::TessellationControl:      return VulkanShaderStage::TessCtrl;
        case spv::ExecutionModel::TessellationEvaluation:   return VulkanShaderStage::TessEval;
        case spv::ExecutionModel::Geometry:                 return VulkanShaderStage::Geometry;
        case spv::ExecutionModel::Fragment:                 return VulkanShaderStage::Fragment;
        case spv::ExecutionModel::GLCompute:                return VulkanShaderStage::Compute;
        case spv::ExecutionModel::RayGenerationKHR:         return VulkanShaderStage::RayGeneration;
        case spv::ExecutionModel::IntersectionKHR:          return VulkanShaderStage::Intersection;
        case spv::ExecutionModel::AnyHitKHR:                return VulkanShaderStage::AnyHit;
        case spv::ExecutionModel::ClosestHitKHR:            return VulkanShaderStage::ClosestHit;
        case spv::ExecutionModel::MissKHR:                  return VulkanShaderStage::Miss;
        case spv::ExecutionModel::CallableKHR:              return VulkanShaderStage::Callable;
        default:                                            return VulkanShaderStage::Invalid;
    }
}

static inline bool IsShaderStageSupported(VulkanShaderStage stage) {
    switch (stage) {
        case VulkanShaderStage::Vertex:
        case VulkanShaderStage::TessCtrl:
        case VulkanShaderStage::TessEval:
        case VulkanShaderStage::Geometry:
        case VulkanShaderStage::Fragment:
        case VulkanShaderStage::Compute:
        case VulkanShaderStage::RayGeneration:
        case VulkanShaderStage::Intersection:
        case VulkanShaderStage::AnyHit:
        case VulkanShaderStage::ClosestHit:
        case VulkanShaderStage::Miss:
        case VulkanShaderStage::Callable: {
            return true;
        }
        default: {
            return false;
        }
    }
}

static inline bool IsPipelineBindPointSupported(VkPipelineBindPoint bindPoint) {
    switch (bindPoint) {
        case VK_PIPELINE_BIND_POINT_GRAPHICS:
        case VK_PIPELINE_BIND_POINT_COMPUTE:
        case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR: {
            return true;
        }
        default: {
            return false;
        }
    }
}

VulkanLayerShaderProfiler::VulkanLayerShaderProfiler(const VulkanLayerShaderProfilerSettings& settings) : VulkanLayerPassThrough(VulkanLayerType::ShaderProfiler), settings_{settings} {}

VulkanLayerShaderProfiler::~VulkanLayerShaderProfiler()
{
    const auto& filename = settings_.filename;

    std::ofstream perfFile(filename);
    if (!perfFile.is_open()) {
        std::cout << "VulkanLayerShaderProfiler::~VulkanLayerShaderProfiler: Could not open file: \'" << filename << "\'\n";
        return;
    }

    std::stringstream stream;
    for (const auto& profileInfo : collectedProfileInfos_) {
        stream << "Pipeline " << profileInfo.pipeline << " (" << GetVulkanPipelineBindPointName(profileInfo.bindPoint) << "):\n";
        for (const auto& shaderInfo : profileInfo.shaderInfos) {
            const auto& profileData = shaderInfo.profileData;

            stream << "    Shader " << shaderInfo.shader << " (" << GetVulkanShaderStageName(shaderInfo.stage) << "): [";
            for (size_t i = 0; i < profileData.size(); ++i) {
                if (i > 0) {
                    stream << ", ";
                }
                stream << profileData[i];
            }
            stream << "]\n";
        }
        stream << "\n";
    }

    perfFile << stream.rdbuf();
    perfFile.close();
}

VkResult VulkanLayerShaderProfiler::vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
    static std::unordered_map<const char*, uint32_t> sExtensionPromotionMap = {
        {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, VK_API_VERSION_1_1},
    };

    VkInstanceCreateInfo modifiedInfo = *pCreateInfo;

    const char* const* extensionsBegin = modifiedInfo.ppEnabledExtensionNames;
    const char* const* extensionsEnd = extensionsBegin + modifiedInfo.enabledExtensionCount;
    std::vector<const char*> extensions(extensionsBegin, extensionsEnd);

    uint32_t apiVersion = modifiedInfo.pApplicationInfo->apiVersion;

    for (const auto& [extName, promotionVersion] : sExtensionPromotionMap) {
        if (apiVersion >= promotionVersion) {
            continue;
        }

        bool hasExtension = false;
        for (const auto& extension : extensions) {
            if (!std::strcmp(extension, extName)) {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension) {
            extensions.push_back(extName);
        }
    }

    modifiedInfo.ppEnabledExtensionNames = extensions.data();
    modifiedInfo.enabledExtensionCount = extensions.size();

    VkResult res = next_->vkCreateInstance(&modifiedInfo, pAllocator, pInstance);
    if (res == VK_SUCCESS) {
        VkInstance instance = *pInstance;

        auto& instanceInfo = instanceInfoMap_[instance];
        instanceInfo.apiVersion = apiVersion;
    }
    return res;
}

void VulkanLayerShaderProfiler::vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
{
    instanceInfoMap_.erase(instance);
    next_->vkDestroyInstance(instance, pAllocator);
}

VkResult VulkanLayerShaderProfiler::vkEnumeratePhysicalDevices(VkInstance instance, uint32_t* pPhysicalDeviceCount, VkPhysicalDevice* pPhysicalDevices)
{
    VkResult res = next_->vkEnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
    if ((res == VK_SUCCESS || res == VK_INCOMPLETE) && pPhysicalDeviceCount && pPhysicalDevices) {
        uint32_t physicalDeviceCount = *pPhysicalDeviceCount;
        for (uint32_t i = 0; i < physicalDeviceCount; ++i) {
            VkPhysicalDevice physicalDevice = pPhysicalDevices[i];

            auto& physicalDeviceInfo = physicalDeviceInfoMap_[physicalDevice];
            physicalDeviceInfo.instance = instance;
            next_->vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceInfo.properties);
            next_->vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceInfo.memoryProperties);

            uint32_t queueFamilyCount = 0;
            next_->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

            physicalDeviceInfo.queueFamilyProperties.resize(queueFamilyCount);
            next_->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, physicalDeviceInfo.queueFamilyProperties.data());
        }
    }
    return res;
}

VkResult VulkanLayerShaderProfiler::vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    static std::unordered_map<const char*, uint32_t> sExtensionPromotionMap = {
        {VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME, VK_API_VERSION_1_1},
        {VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,          VK_API_VERSION_1_2},
    };

    VkDeviceCreateInfo modifiedInfo{};
    SignatureSerializer::Allocator allocator;
    SignatureSerializer::DeepCopy(*pCreateInfo, allocator, modifiedInfo);

    const char* const* extensionsBegin = modifiedInfo.ppEnabledExtensionNames;
    const char* const* extensionsEnd = extensionsBegin + modifiedInfo.enabledExtensionCount;
    std::vector<const char*> extensions(extensionsBegin, extensionsEnd);

    uint32_t apiVersion = VK_API_VERSION_1_0;
    auto physicalDeviceIt = physicalDeviceInfoMap_.find(physicalDevice);
    if (physicalDeviceIt == physicalDeviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::vkCreateDevice: Could not find physical device info\n";
        return VK_ERROR_UNKNOWN;
    }

    const auto& physicalDeviceInfo = physicalDeviceIt->second;
    VkInstance instance = physicalDeviceInfo.instance;

    auto instanceIt = instanceInfoMap_.find(instance);
    if (instanceIt != instanceInfoMap_.end()) {
        const auto& instanceInfo = instanceIt->second;
        apiVersion = instanceInfo.apiVersion;
    }

    for (const auto& [extName, promotionVersion] : sExtensionPromotionMap) {
        if (apiVersion >= promotionVersion) {
            continue;
        }

        bool hasExtension = false;
        for (const auto& extension : extensions) {
            if (!std::strcmp(extension, extName)) {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension) {
            extensions.push_back(extName);
        }
    }

    modifiedInfo.ppEnabledExtensionNames = extensions.data();
    modifiedInfo.enabledExtensionCount = extensions.size();

    bool hasFeatures = false;
    bool hasAtomicFeatures = false;
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceShaderAtomicInt64Features atomicFeatures{};

    // vertexPipelineStoresAndAtomics, fragmentStoresAndAtomics and shaderInt64 features
    const void* pNext = GetStructFromPNextChain(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, modifiedInfo.pNext);
    if (pNext) {
        // It's our own memory, can edit it
        const VkPhysicalDeviceFeatures2* pFeatures2 = reinterpret_cast<const VkPhysicalDeviceFeatures2*>(pNext);
        VkPhysicalDeviceFeatures2& features2 = *const_cast<VkPhysicalDeviceFeatures2*>(pFeatures2);
        features2.features.vertexPipelineStoresAndAtomics = VK_TRUE;
        features2.features.fragmentStoresAndAtomics = VK_TRUE;
        features2.features.shaderInt64 = VK_TRUE;

        hasFeatures = true;
    }

    if (modifiedInfo.pEnabledFeatures) {
        features = *modifiedInfo.pEnabledFeatures;
        features.vertexPipelineStoresAndAtomics = VK_TRUE;
        features.fragmentStoresAndAtomics = VK_TRUE;
        features.shaderInt64 = VK_TRUE;
        modifiedInfo.pEnabledFeatures = &features;

        hasFeatures = true;
    }

    if (!hasFeatures) {
        features.vertexPipelineStoresAndAtomics = VK_TRUE;
        features.fragmentStoresAndAtomics = VK_TRUE;
        features.shaderInt64 = VK_TRUE;
        modifiedInfo.pEnabledFeatures = &features;
    }

    // shaderBufferInt64Atomics feature
    pNext = GetStructFromPNextChain(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES , modifiedInfo.pNext);
    if (pNext) {
        const VkPhysicalDeviceVulkan12Features* pFeatures12 = reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(pNext);
        VkPhysicalDeviceVulkan12Features& features12 = *const_cast<VkPhysicalDeviceVulkan12Features*>(pFeatures12);
        features12.shaderBufferInt64Atomics = VK_TRUE;

        hasAtomicFeatures = true;
    }
    else {
        pNext = GetStructFromPNextChain(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES , modifiedInfo.pNext);
        if (pNext) {
            const VkPhysicalDeviceShaderAtomicInt64Features* pShaderAtomicFeatures = reinterpret_cast<const VkPhysicalDeviceShaderAtomicInt64Features*>(pNext);
            VkPhysicalDeviceShaderAtomicInt64Features& shaderAtomicFeatures = *const_cast<VkPhysicalDeviceShaderAtomicInt64Features*>(pShaderAtomicFeatures);
            shaderAtomicFeatures.shaderBufferInt64Atomics = VK_TRUE;

            hasAtomicFeatures = true;
        }
    }

    if (!hasAtomicFeatures) {
        atomicFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES;
        atomicFeatures.pNext = const_cast<void*>(modifiedInfo.pNext);
        atomicFeatures.shaderBufferInt64Atomics = VK_TRUE;

        modifiedInfo.pNext = &atomicFeatures;
    }

    VkResult res = next_->vkCreateDevice(physicalDevice, &modifiedInfo, pAllocator, pDevice);
    if (res == VK_SUCCESS) {
        VkDevice device = *pDevice;

        auto& deviceInfo = deviceInfoMap_[device];
        deviceInfo.physicalDevice = physicalDevice;

        std::optional<uint32_t> graphicsTransferIndex;
        std::optional<uint32_t> pureTransferIndex;

        const auto& queueFamilyProperties = physicalDeviceInfo.queueFamilyProperties;
        for (uint32_t i = 0; i < modifiedInfo.queueCreateInfoCount; ++i) {
            const auto& queueCreateInfo = modifiedInfo.pQueueCreateInfos[i];
            if (queueCreateInfo.queueCount == 0) {
                continue;
            }

            uint32_t queueFamilyIndex = queueCreateInfo.queueFamilyIndex;
            const auto& familyProperties = queueFamilyProperties[queueFamilyIndex];
            VkQueueFlags queueFlags = familyProperties.queueFlags;

            // Try find transfer only queue
            if (queueFlags & VK_QUEUE_TRANSFER_BIT) {
                if (queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                    graphicsTransferIndex = queueFamilyIndex;
                }
                else {
                    pureTransferIndex = queueFamilyIndex;
                    break;
                }
            }
        }

        uint32_t queueFamilyIndex = 0;
        if (pureTransferIndex) {
            queueFamilyIndex = pureTransferIndex.value();
        }
        else if (graphicsTransferIndex) {
            queueFamilyIndex = graphicsTransferIndex.value();
        }

        deviceInfo.transferQueueFamilyIndex = queueFamilyIndex;
        next_->vkGetDeviceQueue(device, queueFamilyIndex, 0, &deviceInfo.transferQueue);

        PatchDispatchKey(device, deviceInfo.transferQueue);
    }
    return res;
}

void VulkanLayerShaderProfiler::vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    deviceInfoMap_.erase(device);
    next_->vkDestroyDevice(device, pAllocator);
}

VkResult VulkanLayerShaderProfiler::vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule)
{
    VkResult res = next_->vkCreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
    if (res == VK_SUCCESS) {
        VkShaderModule shader = *pShaderModule;

        const uint32_t* codeBegin = pCreateInfo->pCode;
        const uint32_t* codeEnd = codeBegin + pCreateInfo->codeSize / 4;

        auto& shaderInfo = shaderInfoMap_[shader];
        shaderInfo.code.assign(codeBegin, codeEnd);
    }
    return res;
}

void VulkanLayerShaderProfiler::vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator)
{
    shaderInfoMap_.erase(shaderModule);
    next_->vkDestroyShaderModule(device, shaderModule, pAllocator);
}

VkResult VulkanLayerShaderProfiler::vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout)
{
    VkResult res = next_->vkCreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
    if (res == VK_SUCCESS) {
        VkPipelineLayout pipelineLayout = *pPipelineLayout;

        const VkDescriptorSetLayout* setLayoutsBegin = pCreateInfo->pSetLayouts;
        const VkDescriptorSetLayout* setLayoutsEnd = setLayoutsBegin + pCreateInfo->setLayoutCount;

        const VkPushConstantRange* pushConstantRangesBegin = pCreateInfo->pPushConstantRanges;
        const VkPushConstantRange* pushConstantRangesEnd = pushConstantRangesBegin + pCreateInfo->pushConstantRangeCount;

        auto& pipelineLayoutInfo = pipelineLayoutInfoMap_[pipelineLayout];
        pipelineLayoutInfo.setLayouts.assign(setLayoutsBegin, setLayoutsEnd);
        pipelineLayoutInfo.pushConstantRanges.assign(pushConstantRangesBegin, pushConstantRangesEnd);
    }
    return res;
}

void VulkanLayerShaderProfiler::vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator)
{
    pipelineLayoutInfoMap_.erase(pipelineLayout);
    next_->vkDestroyPipelineLayout(device, pipelineLayout, pAllocator);
}

VkResult VulkanLayerShaderProfiler::vkCreateGraphicsPipelines(VkDevice device,
                                                              VkPipelineCache pipelineCache,
                                                              uint32_t createInfoCount,
                                                              const VkGraphicsPipelineCreateInfo* pCreateInfos,
                                                              const VkAllocationCallbacks* pAllocator,
                                                              VkPipeline* pPipelines)
{
    VkResult res = next_->vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    if (res != VK_SUCCESS) {
        return res;
    }

    auto deviceInfoIt = deviceInfoMap_.find(device);
    if (deviceInfoIt == deviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not find device info\n";
        return res;
    }

    const auto& deviceInfo = deviceInfoIt->second;
    VkPhysicalDevice physicalDevice = deviceInfo.physicalDevice;

    auto physicalDeviceInfoIt = physicalDeviceInfoMap_.find(physicalDevice);
    if (physicalDeviceInfoIt == physicalDeviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not find physical device info\n";
        return res;
    }

    const auto& physicalDeviceInfo = physicalDeviceInfoIt->second;
    const auto& physicalDeviceProperties = physicalDeviceInfo.properties;
    const auto maxBoundDescriptorSets = physicalDeviceProperties.limits.maxBoundDescriptorSets;

    for (uint32_t i = 0; i < createInfoCount; ++i) {
        VkPipeline origPipeline = pPipelines[i];
        VkPipelineLayout origLayout = pCreateInfos[i].layout;
        VkGraphicsPipelineCreateInfo modifiedCreateInfo = pCreateInfos[i];

        auto pipelineLayoutInfoIt = pipelineLayoutInfoMap_.find(origLayout);
        if (pipelineLayoutInfoIt == pipelineLayoutInfoMap_.end()) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not find pipeline layout info\n";
            continue;
        }

        const auto& pipelineLayoutInfo = pipelineLayoutInfoIt->second;
        const auto& setLayouts = pipelineLayoutInfo.setLayouts;

        if (setLayouts.size() == maxBoundDescriptorSets) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Cannot profile pipeline because all it uses all possible set layouts\n";
            continue;
        }

        uint32_t profileSet = setLayouts.size();

        const VkPipelineShaderStageCreateInfo* stageInfosBegin = modifiedCreateInfo.pStages;
        const VkPipelineShaderStageCreateInfo* stageInfosEnd = stageInfosBegin + modifiedCreateInfo.stageCount;
        std::vector<VkPipelineShaderStageCreateInfo> stageInfos(stageInfosBegin, stageInfosEnd);

        std::vector<ShaderProfileInfo> shaderProfileInfos;
        if (!CreateShaderProfileInfos(device, pAllocator, stageInfos, profileSet, shaderProfileInfos)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not create shader profile infos\n";
            continue;
        }

        VkDescriptorSetLayout profileSetLayout = VK_NULL_HANDLE;
        if (!CreateProfileDescriptorSetLayout(device, pAllocator, shaderProfileInfos, profileSetLayout)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not create profile descriptor set layout\n";
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        VkDescriptorPool profileDescriptorPool = VK_NULL_HANDLE;
        if (!CreateProfileDescriptorPool(device, pAllocator, shaderProfileInfos, profileDescriptorPool)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not create profile descriptor pool\n";
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        VkDescriptorSet profileDescriptorSet = VK_NULL_HANDLE;
        if (!SetupProfileDescriptorSet(device, shaderProfileInfos, profileSetLayout, profileDescriptorPool, profileDescriptorSet)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not setup profile descriptor set\n";
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        VkPipelineLayout modifiedLayout = VK_NULL_HANDLE;
        if (!CreateProfilePipelineLayout(device, pAllocator, shaderProfileInfos, origLayout, profileSetLayout, modifiedLayout)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not create profile pipeline layout\n";
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        PipelineProfileCommandInfo commandInfo;
        if (!CreatePipelineProfileCommandInfo(device, pAllocator, commandInfo)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not create profile command info\n";
            DestroyProfilePipelineLayout(device, pAllocator, modifiedLayout);
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        for (uint32_t i = 0; i < stageInfos.size(); ++i) {
            auto& stageInfo = stageInfos[i];
            auto& shaderInfo = shaderProfileInfos[i];
            stageInfo.module = shaderInfo.modifiedShader;
        }

        modifiedCreateInfo.pStages = stageInfos.data();
        modifiedCreateInfo.layout = modifiedLayout;

        VkPipeline modifiedPipeline = VK_NULL_HANDLE;
        VkResult vkres = next_->vkCreateGraphicsPipelines(device, pipelineCache, 1, &modifiedCreateInfo, pAllocator, &modifiedPipeline);
        if (vkres != VK_SUCCESS) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateGraphicsPipelines: Could not create profile pipeline\n";
            DestroyPipelineProfileCommandInfo(device, pAllocator, commandInfo);
            DestroyProfilePipelineLayout(device, pAllocator, modifiedLayout);
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        auto& pipelineProfileInfo = pipelineProfileInfoMap_[origPipeline];
        pipelineProfileInfo.device = device;
        pipelineProfileInfo.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        pipelineProfileInfo.origPipeline = origPipeline;
        pipelineProfileInfo.modifiedPipeline = modifiedPipeline;
        pipelineProfileInfo.origLayout = origLayout;
        pipelineProfileInfo.modifiedLayout = modifiedLayout;
        pipelineProfileInfo.profileSetLayout = profileSetLayout;
        pipelineProfileInfo.profileDescriptorPool = profileDescriptorPool;
        pipelineProfileInfo.profileDescriptorSet = profileDescriptorSet;
        pipelineProfileInfo.commandInfo = commandInfo;
        pipelineProfileInfo.shaderInfos = std::move(shaderProfileInfos);
    }

    return res;
}

VkResult VulkanLayerShaderProfiler::vkCreateComputePipelines(VkDevice device,
                                                             VkPipelineCache pipelineCache,
                                                             uint32_t createInfoCount,
                                                             const VkComputePipelineCreateInfo* pCreateInfos,
                                                             const VkAllocationCallbacks* pAllocator,
                                                             VkPipeline* pPipelines)
{
    VkResult res = next_->vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    if (res != VK_SUCCESS) {
        return res;
    }

    auto deviceInfoIt = deviceInfoMap_.find(device);
    if (deviceInfoIt == deviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not find device info\n";
        return res;
    }

    const auto& deviceInfo = deviceInfoIt->second;
    VkPhysicalDevice physicalDevice = deviceInfo.physicalDevice;

    auto physicalDeviceInfoIt = physicalDeviceInfoMap_.find(physicalDevice);
    if (physicalDeviceInfoIt == physicalDeviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not find physical device info\n";
        return res;
    }

    const auto& physicalDeviceInfo = physicalDeviceInfoIt->second;
    const auto& physicalDeviceProperties = physicalDeviceInfo.properties;
    const auto maxBoundDescriptorSets = physicalDeviceProperties.limits.maxBoundDescriptorSets;

    for (uint32_t i = 0; i < createInfoCount; ++i) {
        VkPipeline origPipeline = pPipelines[i];
        VkPipelineLayout origLayout = pCreateInfos[i].layout;
        VkComputePipelineCreateInfo modifiedCreateInfo = pCreateInfos[i];

        auto pipelineLayoutInfoIt = pipelineLayoutInfoMap_.find(origLayout);
        if (pipelineLayoutInfoIt == pipelineLayoutInfoMap_.end()) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not find pipeline layout info\n";
            continue;
        }

        const auto& pipelineLayoutInfo = pipelineLayoutInfoIt->second;
        const auto& setLayouts = pipelineLayoutInfo.setLayouts;

        if (setLayouts.size() == maxBoundDescriptorSets) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Cannot profile pipeline because all it uses all possible set layouts\n";
            continue;
        }

        uint32_t profileSet = setLayouts.size();

        std::vector<VkPipelineShaderStageCreateInfo> stageInfos = { modifiedCreateInfo.stage };

        std::vector<ShaderProfileInfo> shaderProfileInfos;
        if (!CreateShaderProfileInfos(device, pAllocator, stageInfos, profileSet, shaderProfileInfos)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not create shader profile infos\n";
            continue;
        }

        VkDescriptorSetLayout profileSetLayout = VK_NULL_HANDLE;
        if (!CreateProfileDescriptorSetLayout(device, pAllocator, shaderProfileInfos, profileSetLayout)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not create profile descriptor set layout\n";
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        VkDescriptorPool profileDescriptorPool = VK_NULL_HANDLE;
        if (!CreateProfileDescriptorPool(device, pAllocator, shaderProfileInfos, profileDescriptorPool)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not create profile descriptor pool\n";
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        VkDescriptorSet profileDescriptorSet = VK_NULL_HANDLE;
        if (!SetupProfileDescriptorSet(device, shaderProfileInfos, profileSetLayout, profileDescriptorPool, profileDescriptorSet)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not setup profile descriptor set\n";
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        VkPipelineLayout modifiedLayout = VK_NULL_HANDLE;
        if (!CreateProfilePipelineLayout(device, pAllocator, shaderProfileInfos, origLayout, profileSetLayout, modifiedLayout)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not create profile pipeline layout\n";
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        PipelineProfileCommandInfo commandInfo;
        if (!CreatePipelineProfileCommandInfo(device, pAllocator, commandInfo)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not create profile command info\n";
            DestroyProfilePipelineLayout(device, pAllocator, modifiedLayout);
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        auto& stageInfo = stageInfos[0];
        auto& shaderInfo = shaderProfileInfos[0];
        stageInfo.module = shaderInfo.modifiedShader;

        modifiedCreateInfo.stage = stageInfo;
        modifiedCreateInfo.layout = modifiedLayout;

        VkPipeline modifiedPipeline = VK_NULL_HANDLE;
        VkResult vkres = next_->vkCreateComputePipelines(device, pipelineCache, 1, &modifiedCreateInfo, pAllocator, &modifiedPipeline);
        if (vkres != VK_SUCCESS) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateComputePipelines: Could not create profile pipeline\n";
            DestroyPipelineProfileCommandInfo(device, pAllocator, commandInfo);
            DestroyProfilePipelineLayout(device, pAllocator, modifiedLayout);
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        auto& pipelineProfileInfo = pipelineProfileInfoMap_[origPipeline];
        pipelineProfileInfo.device = device;
        pipelineProfileInfo.bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
        pipelineProfileInfo.origPipeline = origPipeline;
        pipelineProfileInfo.modifiedPipeline = modifiedPipeline;
        pipelineProfileInfo.origLayout = origLayout;
        pipelineProfileInfo.modifiedLayout = modifiedLayout;
        pipelineProfileInfo.profileSetLayout = profileSetLayout;
        pipelineProfileInfo.profileDescriptorPool = profileDescriptorPool;
        pipelineProfileInfo.profileDescriptorSet = profileDescriptorSet;
        pipelineProfileInfo.commandInfo = commandInfo;
        pipelineProfileInfo.shaderInfos = std::move(shaderProfileInfos);
    }

    return res;
}

VkResult VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR(VkDevice device,
                                                                   VkDeferredOperationKHR deferredOperation,
                                                                   VkPipelineCache pipelineCache,
                                                                   uint32_t createInfoCount,
                                                                   const VkRayTracingPipelineCreateInfoKHR* pCreateInfos,
                                                                   const VkAllocationCallbacks* pAllocator,
                                                                   VkPipeline* pPipelines)
{
    VkResult res = next_->vkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    if (res != VK_SUCCESS) {
        return res;
    }

    auto deviceInfoIt = deviceInfoMap_.find(device);
    if (deviceInfoIt == deviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not find device info\n";
        return res;
    }

    const auto& deviceInfo = deviceInfoIt->second;
    VkPhysicalDevice physicalDevice = deviceInfo.physicalDevice;

    auto physicalDeviceInfoIt = physicalDeviceInfoMap_.find(physicalDevice);
    if (physicalDeviceInfoIt == physicalDeviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not find physical device info\n";
        return res;
    }

    const auto& physicalDeviceInfo = physicalDeviceInfoIt->second;
    const auto& physicalDeviceProperties = physicalDeviceInfo.properties;
    const auto maxBoundDescriptorSets = physicalDeviceProperties.limits.maxBoundDescriptorSets;

    for (uint32_t i = 0; i < createInfoCount; ++i) {
        VkPipeline origPipeline = pPipelines[i];
        VkPipelineLayout origLayout = pCreateInfos[i].layout;
        VkRayTracingPipelineCreateInfoKHR modifiedCreateInfo = pCreateInfos[i];

        auto pipelineLayoutInfoIt = pipelineLayoutInfoMap_.find(origLayout);
        if (pipelineLayoutInfoIt == pipelineLayoutInfoMap_.end()) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not find pipeline layout info\n";
            continue;
        }

        const auto& pipelineLayoutInfo = pipelineLayoutInfoIt->second;
        const auto& setLayouts = pipelineLayoutInfo.setLayouts;

        if (setLayouts.size() == maxBoundDescriptorSets) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Cannot profile pipeline because all it uses all possible set layouts\n";
            continue;
        }

        uint32_t profileSet = setLayouts.size();

        const VkPipelineShaderStageCreateInfo* stageInfosBegin = modifiedCreateInfo.pStages;
        const VkPipelineShaderStageCreateInfo* stageInfosEnd = stageInfosBegin + modifiedCreateInfo.stageCount;
        std::vector<VkPipelineShaderStageCreateInfo> stageInfos(stageInfosBegin, stageInfosEnd);

        std::vector<ShaderProfileInfo> shaderProfileInfos;
        if (!CreateShaderProfileInfos(device, pAllocator, stageInfos, profileSet, shaderProfileInfos)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not create shader profile infos\n";
            continue;
        }

        VkDescriptorSetLayout profileSetLayout = VK_NULL_HANDLE;
        if (!CreateProfileDescriptorSetLayout(device, pAllocator, shaderProfileInfos, profileSetLayout)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not create profile descriptor set layout\n";
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        VkDescriptorPool profileDescriptorPool = VK_NULL_HANDLE;
        if (!CreateProfileDescriptorPool(device, pAllocator, shaderProfileInfos, profileDescriptorPool)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not create profile descriptor pool\n";
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        VkDescriptorSet profileDescriptorSet = VK_NULL_HANDLE;
        if (!SetupProfileDescriptorSet(device, shaderProfileInfos, profileSetLayout, profileDescriptorPool, profileDescriptorSet)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not setup profile descriptor set\n";
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        VkPipelineLayout modifiedLayout = VK_NULL_HANDLE;
        if (!CreateProfilePipelineLayout(device, pAllocator, shaderProfileInfos, origLayout, profileSetLayout, modifiedLayout)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not create profile pipeline layout\n";
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        PipelineProfileCommandInfo commandInfo;
        if (!CreatePipelineProfileCommandInfo(device, pAllocator, commandInfo)) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not create profile command info\n";
            DestroyProfilePipelineLayout(device, pAllocator, modifiedLayout);
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        for (uint32_t i = 0; i < stageInfos.size(); ++i) {
            auto& stageInfo = stageInfos[i];
            auto& shaderInfo = shaderProfileInfos[i];
            stageInfo.module = shaderInfo.modifiedShader;
        }

        modifiedCreateInfo.pStages = stageInfos.data();
        modifiedCreateInfo.layout = modifiedLayout;

        VkPipeline modifiedPipeline = VK_NULL_HANDLE;
        VkResult vkres = next_->vkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, 1, &modifiedCreateInfo, pAllocator, &modifiedPipeline);
        if (vkres != VK_SUCCESS) {
            std::cout << "VulkanLayerShaderProfiler::vkCreateRayTracingPipelinesKHR: Could not create profile pipeline\n";
            DestroyPipelineProfileCommandInfo(device, pAllocator, commandInfo);
            DestroyProfilePipelineLayout(device, pAllocator, modifiedLayout);
            DestroyProfileDescriptorPool(device, pAllocator, profileDescriptorPool);
            DestroyProfileDescriptorSetLayout(device, pAllocator, profileSetLayout);
            DestroyShaderProfileInfos(device, pAllocator, shaderProfileInfos);
            continue;
        }

        auto& pipelineProfileInfo = pipelineProfileInfoMap_[origPipeline];
        pipelineProfileInfo.device = device;
        pipelineProfileInfo.bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        pipelineProfileInfo.origPipeline = origPipeline;
        pipelineProfileInfo.modifiedPipeline = modifiedPipeline;
        pipelineProfileInfo.origLayout = origLayout;
        pipelineProfileInfo.modifiedLayout = modifiedLayout;
        pipelineProfileInfo.profileSetLayout = profileSetLayout;
        pipelineProfileInfo.profileDescriptorPool = profileDescriptorPool;
        pipelineProfileInfo.profileDescriptorSet = profileDescriptorSet;
        pipelineProfileInfo.commandInfo = commandInfo;
        pipelineProfileInfo.shaderInfos = std::move(shaderProfileInfos);
    }

    return res;
}

void VulkanLayerShaderProfiler::vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator)
{
    auto it = pipelineProfileInfoMap_.find(pipeline);
    if (it != pipelineProfileInfoMap_.end()) {
        const auto& pipelineProfileInfo = it->second;

        CollectProfileData(device, pipelineProfileInfo);
        next_->vkDestroyPipeline(device, pipelineProfileInfo.modifiedPipeline, pAllocator);
        DestroyPipelineProfileCommandInfo(device, pAllocator, pipelineProfileInfo.commandInfo);
        DestroyProfilePipelineLayout(device, pAllocator, pipelineProfileInfo.modifiedLayout);
        DestroyProfileDescriptorPool(device, pAllocator, pipelineProfileInfo.profileDescriptorPool);
        DestroyProfileDescriptorSetLayout(device, pAllocator, pipelineProfileInfo.profileSetLayout);
        DestroyShaderProfileInfos(device, pAllocator, pipelineProfileInfo.shaderInfos);

        pipelineProfileInfoMap_.erase(it);
    }
    next_->vkDestroyPipeline(device, pipeline, pAllocator);
}

VkResult VulkanLayerShaderProfiler::vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData)
{
    auto it = pipelineProfileInfoMap_.find(pipeline);
    if (it != pipelineProfileInfoMap_.end()) {
        const auto& pipelineProfileInfo = it->second;

        pipeline = pipelineProfileInfo.modifiedPipeline;
    }
    return next_->vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VkResult VulkanLayerShaderProfiler::vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData)
{
    auto it = pipelineProfileInfoMap_.find(pipeline);
    if (it != pipelineProfileInfoMap_.end()) {
        const auto& pipelineProfileInfo = it->second;

        pipeline = pipelineProfileInfo.modifiedPipeline;
    }
    return next_->vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VkDeviceSize VulkanLayerShaderProfiler::vkGetRayTracingShaderGroupStackSizeKHR(VkDevice device, VkPipeline pipeline, uint32_t group, VkShaderGroupShaderKHR groupShader)
{
    auto it = pipelineProfileInfoMap_.find(pipeline);
    if (it != pipelineProfileInfoMap_.end()) {
        const auto& pipelineProfileInfo = it->second;

        pipeline = pipelineProfileInfo.modifiedPipeline;
    }
    return next_->vkGetRayTracingShaderGroupStackSizeKHR(device, pipeline, group, groupShader);
}

void VulkanLayerShaderProfiler::vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
{
    if (!BindProfilePipeline(commandBuffer, pipelineBindPoint, pipeline)) {
        std::cout << "VulkanLayerShaderProfiler::vkCmdBindPipeline: Could not bind profile pipeline. Fallback to original pipeline\n";
        next_->vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
    }
}

bool VulkanLayerShaderProfiler::CreateShaderProfileInfos(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<VkPipelineShaderStageCreateInfo>& stageInfos, uint32_t profileSet, std::vector<ShaderProfileInfo>& shaderInfosOut) const
{
    uint32_t profileBinding = 0;
    std::vector<ShaderProfileInfo> shaderInfos;
    for (const auto& stageInfo : stageInfos) {
        ShaderProfileInfo shaderInfo{};
        if (!CreateShaderProfileInfo(device, pAllocator, stageInfo, profileSet, profileBinding, shaderInfo)) {
            std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileInfos: Could not create shader profile info\n";
            DestroyShaderProfileInfos(device, pAllocator, shaderInfos);
            return false;
        }

        ++profileBinding;
        shaderInfos.emplace_back(std::move(shaderInfo));
    }

    shaderInfosOut = std::move(shaderInfos);
    return true;
}

void VulkanLayerShaderProfiler::DestroyShaderProfileInfos(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<ShaderProfileInfo>& shaderInfos) const
{
    for (const auto& shaderInfo : shaderInfos) {
        DestroyShaderProfileInfo(device, pAllocator, shaderInfo);
    }
}

bool VulkanLayerShaderProfiler::CreateShaderProfileInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const VkPipelineShaderStageCreateInfo& stageInfo, uint32_t profileSet, uint32_t profileBinding, ShaderProfileInfo& shaderInfoOut) const
{
    VkShaderModule origShader = stageInfo.module;

    auto shaderInfoIt = shaderInfoMap_.find(origShader);
    if (shaderInfoIt == shaderInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileInfo: Could not find shader info\n";
        return false;
    }

    const auto& shaderInfo = shaderInfoIt->second;
    const auto& origCode = shaderInfo.code;

    uint32_t bbCount = 0;
    VulkanShaderStage stage = VulkanShaderStage::Invalid;
    std::vector<uint32_t> modifiedCode;
    if (!ModifySPIRV(origCode, profileSet, profileBinding, modifiedCode, bbCount, stage)) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileInfo: Could not modify SPIRV\n";
        return false;
    }

    VkShaderModule modifiedShader = VK_NULL_HANDLE;
    if (!CreateShaderProfileShader(device, pAllocator, modifiedCode, modifiedShader)) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileInfo: Could not create modified shader\n";
        return false;
    }

    ShaderProfileStorage storage;
    if (!CreateShaderProfileStorage(device, pAllocator, bbCount, storage)) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileInfo: Could not create shader profile storage\n";
        DestroyShaderProfileShader(device, pAllocator, modifiedShader);
        return false;
    }

    shaderInfoOut.stage = stage;
    shaderInfoOut.origShader = origShader;
    shaderInfoOut.modifiedShader = modifiedShader;
    shaderInfoOut.origCode = origCode;
    shaderInfoOut.modifiedCode = std::move(modifiedCode);
    shaderInfoOut.shaderBBCount = bbCount;
    shaderInfoOut.profileSet = profileSet;
    shaderInfoOut.profileBinding = profileBinding;
    shaderInfoOut.storage = storage;
    return true;
}

void VulkanLayerShaderProfiler::DestroyShaderProfileInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const ShaderProfileInfo& shaderInfo) const
{
    DestroyShaderProfileStorage(device, pAllocator, shaderInfo.storage);
    DestroyShaderProfileShader(device, pAllocator, shaderInfo.modifiedShader);
}

bool VulkanLayerShaderProfiler::ModifySPIRV(const std::vector<uint32_t>& origCode, uint32_t profileSet, uint32_t profileBinding, std::vector<uint32_t>& modifiedCodeOut, uint32_t& bbCountOut, VulkanShaderStage& stageOut) const
{
    spvtools::SpirvTools core(SPV_ENV_VULKAN_1_4);
    core.SetMessageConsumer(&SPIRVErrorHandler);

    auto context = spvtools::BuildModule(SPV_ENV_VULKAN_1_4, &SPIRVErrorHandler, origCode.data(), origCode.size(), true);
    if (!context) {
        std::cout << "SPIRV: Could not build module\n";
        return false;
    }

    auto executionModel = context->GetStage();
    auto stage = ExecutionModelToShaderStage(executionModel);
    if (!IsShaderStageSupported(stage)) {
        std::cout << "SPIRV: Creating shader module for unsupported stage \'" << GetVulkanShaderStageName(stage) << "\'. Skipping...\n";
        return false;
    }

    auto& m = *context->module();

    uint32_t bbCount = 0;
    for (const auto& f : m) {
        bbCount += f.end() - f.begin();
    }

    if (bbCount == 0) {
        std::cout << "SPIRV: Shader module does not have any basic blocks\n";
        return false;
    }

    using Instruction = spvtools::opt::Instruction;
    using Operand = spvtools::opt::Operand;

    Instruction* uint32TypeInst = nullptr;
    Instruction* int32TypeInst = nullptr;
    Instruction* uint64TypeInst = nullptr;
    Instruction* const0Uint32Inst = nullptr;
    Instruction* const1Uint32Inst = nullptr;
    Instruction* constBBCountUint32Inst = nullptr;
    std::vector<Instruction*> constIdxInsts(bbCount);
    Instruction* arrayTypeInst = nullptr;
    Instruction* structTypeInst = nullptr;
    Instruction* structPtrTypeInst = nullptr;
    Instruction* arrayPtrTypeInst = nullptr;
    Instruction* structPtrInst = nullptr;

    for (auto* inst : m.GetTypes()) {
        if (inst->opcode() != spv::Op::OpTypeInt) {
            continue;
        }

        const auto& opWidth = inst->GetOperand(1);
        const auto& opSign = inst->GetOperand(2);
        uint64_t width = opWidth.AsLiteralUint64();
        uint64_t sign = opSign.AsLiteralUint64();
        if (width == 32) {
            if (sign == 0) {
                uint32TypeInst = inst;
            }
            else {
                int32TypeInst = inst;
            }
        }
        else if (width == 64) {
            if (sign == 0) {
                uint64TypeInst = inst;
            }
        }
    }

    // OpCapability Int64
    if (!m.HasExplicitCapability(uint32_t(spv::Capability::Int64))) {
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_CAPABILITY, {uint32_t(spv::Capability::Int64)}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpCapability, 0, 0, operands);

        m.AddCapability(std::move(inst));
    }

    // OpCapability Int64Atomics
    if (!m.HasExplicitCapability(uint32_t(spv::Capability::Int64Atomics))) {
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_CAPABILITY, {uint32_t(spv::Capability::Int64Atomics)}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpCapability, 0, 0, operands);

        m.AddCapability(std::move(inst));
    }

    // OpExtension "SPV_KHR_storage_buffer_storage_class"
    if (m.version() < SPV_SPIRV_VERSION_WORD(1, 3))
    {
        const std::string extension = "SPV_KHR_storage_buffer_storage_class";
        bool hasExtension = false;
        for (const auto& inst : m.extensions()) {
            const auto& opName = inst.GetOperand(0);
            if (opName.AsString() == extension) {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension) {
            std::vector<uint32_t> words = spvtools::utils::MakeVector(extension);
            auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_LITERAL_STRING, {words}}};
            auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpExtension, 0, 0, operands);

            m.AddExtension(std::move(inst));
        }
    }

    // %uint = OpTypeInt 32 0
    if (uint32TypeInst == nullptr)
    {
        uint32_t resultId = context->TakeNextId();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_LITERAL_INTEGER, {32}}, {SPV_OPERAND_TYPE_LITERAL_INTEGER, {0}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpTypeInt, 0, resultId, operands);
        uint32TypeInst = inst.get();

        m.AddType(std::move(inst));
    }

    // %int = OpTypeInt 32 1
    if (int32TypeInst == nullptr)
    {
        uint32_t resultId = context->TakeNextId();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_LITERAL_INTEGER, {32}}, {SPV_OPERAND_TYPE_LITERAL_INTEGER, {1}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpTypeInt, 0, resultId, operands);
        int32TypeInst = inst.get();

        m.AddType(std::move(inst));
    }

    // %uint64 = OpTypeInt 64 0
    if (uint64TypeInst == nullptr)
    {
        uint32_t resultId = context->TakeNextId();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_LITERAL_INTEGER, {64}}, {SPV_OPERAND_TYPE_LITERAL_INTEGER, {0}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpTypeInt, 0, resultId, operands);
        uint64TypeInst = inst.get();

        m.AddType(std::move(inst));
    }

    // %uint_0 = OpConstant %uint 0
    {
        uint32_t typeId = uint32TypeInst->result_id();
        uint32_t resultId = context->TakeNextId();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_LITERAL_INTEGER, {0}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpConstant, typeId, resultId, operands);
        const0Uint32Inst = inst.get();

        m.AddGlobalValue(std::move(inst));
    }

    // %uint_1 = OpConstant %uint 1
    {
        uint32_t typeId = uint32TypeInst->result_id();
        uint32_t resultId = context->TakeNextId();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_LITERAL_INTEGER, {1}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpConstant, typeId, resultId, operands);
        const1Uint32Inst = inst.get();

        m.AddGlobalValue(std::move(inst));
    }

    // %uint_<size> = OpConstant %uint <size>
    {
        if (bbCount == 1) {
            constBBCountUint32Inst = const1Uint32Inst;
        }
        else {
            uint32_t typeId = uint32TypeInst->result_id();
            uint32_t resultId = context->TakeNextId();
            auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_LITERAL_INTEGER, {bbCount}}};
            auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpConstant, typeId, resultId, operands);
            constBBCountUint32Inst = inst.get();

            m.AddGlobalValue(std::move(inst));
        }
    }

    // %int_<idx> = OpConstant %int <idx>
    {
        for (uint32_t i = 0; i < bbCount; ++i) {
            uint32_t typeId = int32TypeInst->result_id();
            uint32_t resultId = context->TakeNextId();
            auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_LITERAL_INTEGER, {i}}};
            auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpConstant, typeId, resultId, operands);
            constIdxInsts[i] = inst.get();

            m.AddGlobalValue(std::move(inst));
        }
    }

    // %arr = OpTypeArray %uint64 %uint_<size>
    {
        uint32_t typeId = uint64TypeInst->result_id();
        uint32_t resultId = context->TakeNextId();
        uint32_t countId = constBBCountUint32Inst->result_id();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_ID, {typeId}}, {SPV_OPERAND_TYPE_ID, {countId}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpTypeArray, 0, resultId, operands);
        arrayTypeInst = inst.get();

        m.AddType(std::move(inst));
    }

    // %struct_buffer = OpTypeStruct %arr
    {
        uint32_t memberTypeId = arrayTypeInst->result_id();
        uint32_t resultId = context->TakeNextId();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_ID, {memberTypeId}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpTypeStruct, 0, resultId, operands);
        structTypeInst = inst.get();

        m.AddType(std::move(inst));
    }

    // %struct_buffer_ptr = OpTypePointer StorageBuffer %struct_buffer
    {
        uint32_t typeId = structTypeInst->result_id();
        uint32_t resultId = context->TakeNextId();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_STORAGE_CLASS, {uint32_t(spv::StorageClass::StorageBuffer)}}, {SPV_OPERAND_TYPE_ID, {typeId}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpTypePointer, 0, resultId, operands);
        structPtrTypeInst = inst.get();

        m.AddType(std::move(inst));
    }

    // %arr_ptr = OpTypePointer StorageBuffer %uint64
    {
        uint32_t typeId = uint64TypeInst->result_id();
        uint32_t resultId = context->TakeNextId();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_STORAGE_CLASS, {uint32_t(spv::StorageClass::StorageBuffer)}}, {SPV_OPERAND_TYPE_ID, {typeId}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpTypePointer, 0, resultId, operands);
        arrayPtrTypeInst = inst.get();

        m.AddType(std::move(inst));
    }

    // %struct_buffer_ptr_var = OpVariable %struct_buffer_ptr StorageBuffer
    {
        uint32_t typeId = structPtrTypeInst->result_id();
        uint32_t resultId = context->TakeNextId();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_STORAGE_CLASS, {uint32_t(spv::StorageClass::StorageBuffer)}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpVariable, typeId, resultId, operands);
        structPtrInst = inst.get();

        m.AddGlobalValue(std::move(inst));
    }

    // OpDecorate %arr ArrayStride 8
    {
        uint32_t targetId = arrayTypeInst->result_id();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_ID, {targetId}}, {SPV_OPERAND_TYPE_DECORATION, {uint32_t(spv::Decoration::ArrayStride)}}, {SPV_OPERAND_TYPE_LITERAL_INTEGER, {8}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpDecorate, 0, 0, operands);

        m.AddAnnotationInst(std::move(inst));
    }

    // OpMemberDecorate %struct_buffer 0 Offset 0
    {
        uint32_t targetId = structTypeInst->result_id();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_ID, {targetId}}, {SPV_OPERAND_TYPE_LITERAL_INTEGER, {0}}, {SPV_OPERAND_TYPE_DECORATION, {uint32_t(spv::Decoration::Offset)}}, {SPV_OPERAND_TYPE_LITERAL_INTEGER, {0}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpMemberDecorate, 0, 0, operands);

        m.AddAnnotationInst(std::move(inst));
    }

    // OpDecorate %struct_buffer Block
    {
        uint32_t targetId = structTypeInst->result_id();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_ID, {targetId}}, {SPV_OPERAND_TYPE_DECORATION, {uint32_t(spv::Decoration::Block)}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpDecorate, 0, 0, operands);

        m.AddAnnotationInst(std::move(inst));
    }

    // OpDecorate %struct_buffer_ptr_var DescriptorSet <set>
    {
        uint32_t targetId = structPtrInst->result_id();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_ID, {targetId}}, {SPV_OPERAND_TYPE_DECORATION, {uint32_t(spv::Decoration::DescriptorSet)}}, {SPV_OPERAND_TYPE_LITERAL_INTEGER, {profileSet}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpDecorate, 0, 0, operands);

        m.AddAnnotationInst(std::move(inst));
    }

    // OpDecorate %struct_buffer_ptr_var Binding <binding>
    {
        uint32_t targetId = structPtrInst->result_id();
        auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_ID, {targetId}}, {SPV_OPERAND_TYPE_DECORATION, {uint32_t(spv::Decoration::Binding)}}, {SPV_OPERAND_TYPE_LITERAL_INTEGER, {profileBinding}}};
        auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpDecorate, 0, 0, operands);

        m.AddAnnotationInst(std::move(inst));
    }

    if (m.version() > SPV_SPIRV_VERSION_WORD(1, 3))
    {
        uint32_t targetId = structPtrInst->result_id();
        for (auto& inst : m.entry_points()) {
            spvtools::opt::Operand operand(SPV_OPERAND_TYPE_ID, {targetId});
            inst.AddOperand(operand);
        }
    }

    uint32_t bbIdx = 0;
    for (auto& f : m) {
        for (auto& bb : f) {
            Instruction* firstInst = &(*bb.begin());
            while (firstInst->opcode() == spv::Op::OpLabel
                || firstInst->opcode() == spv::Op::OpPhi
                || firstInst->opcode() == spv::Op::OpVariable
                || firstInst->opcode() == spv::Op::OpUntypedVariableKHR)
            {
                firstInst = firstInst->NextNode();
            }

            Instruction* accessChainInst = nullptr;
            {
                uint32_t typeId = arrayPtrTypeInst->result_id();
                uint32_t resultId = context->TakeNextId();
                uint32_t varId = structPtrInst->result_id();
                uint32_t int0Id = constIdxInsts[0]->result_id();
                uint32_t idxId = constIdxInsts[bbIdx]->result_id();
                auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_ID, {varId}}, {SPV_OPERAND_TYPE_ID, {int0Id}}, {SPV_OPERAND_TYPE_ID, {idxId}}};
                auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpAccessChain, typeId, resultId, operands);
                accessChainInst = inst.get();

                firstInst->InsertBefore(std::move(inst));
            }

            {
                uint32_t typeId = uint64TypeInst->result_id();
                uint32_t resultId = context->TakeNextId();
                uint32_t varId = accessChainInst->result_id();
                uint32_t uint0Id = const0Uint32Inst->result_id();
                uint32_t uint1Id = const1Uint32Inst->result_id();
                auto operands = Instruction::OperandList{{SPV_OPERAND_TYPE_ID, {varId}}, {SPV_OPERAND_TYPE_ID, {uint1Id}}, {SPV_OPERAND_TYPE_ID, {uint0Id}}};
                auto inst = std::make_unique<Instruction>(context.get(), spv::Op::OpAtomicIIncrement, typeId, resultId, operands);

                firstInst->InsertBefore(std::move(inst));
            }

            ++bbIdx;
        }
    }

    std::vector<uint32_t> binary;
    m.ToBinary(&binary, false);

    if (!core.Validate(binary)) {
        std::cout << "SPIRV: Modified SPIRV code is invalid\n";
        return false;
    }

    modifiedCodeOut = std::move(binary);
    bbCountOut = bbCount;
    stageOut = stage;
    return true;
}

bool VulkanLayerShaderProfiler::CreateShaderProfileShader(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<uint32_t>& modifiedCode, VkShaderModule& shaderOut) const
{
    VkShaderModuleCreateInfo smci{};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.pCode = modifiedCode.data();
    smci.codeSize = modifiedCode.size() * 4;

    VkShaderModule modifiedShader = VK_NULL_HANDLE;
    VkResult vkres = next_->vkCreateShaderModule(device, &smci, pAllocator, &modifiedShader);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileShader: Could not create modified shader\n";
        return false;
    }

    shaderOut = modifiedShader;
    return true;
}

void VulkanLayerShaderProfiler::DestroyShaderProfileShader(VkDevice device, const VkAllocationCallbacks* pAllocator, VkShaderModule shader) const
{
    next_->vkDestroyShaderModule(device, shader, pAllocator);
}

bool VulkanLayerShaderProfiler::CreateShaderProfileStorage(VkDevice device, const VkAllocationCallbacks* pAllocator, uint32_t bbCount, ShaderProfileStorage& storageOut) const
{
    auto deviceInfoIt = deviceInfoMap_.find(device);
    if (deviceInfoIt == deviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not find device info\n";
        return false;
    }

    const auto& deviceInfo = deviceInfoIt->second;
    VkPhysicalDevice physicalDevice = deviceInfo.physicalDevice;

    auto physicalDeviceInfoIt = physicalDeviceInfoMap_.find(physicalDevice);
    if (physicalDeviceInfoIt == physicalDeviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not find physical device info\n";
        return false;
    }

    const auto& physicalDeviceInfo = physicalDeviceInfoIt->second;
    const auto& memoryProperties = physicalDeviceInfo.memoryProperties;

    ShaderProfileStorage storage;

    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.size = bbCount * sizeof(uint64_t);

    VkResult vkres = next_->vkCreateBuffer(device, &bci, pAllocator, &storage.localBuffer);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not create local buffer for shader\n";
        DestroyShaderProfileStorage(device, pAllocator, storage);
        return false;
    }

    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    vkres = next_->vkCreateBuffer(device, &bci, pAllocator, &storage.stagingBuffer);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not create staging buffer for shader\n";
        DestroyShaderProfileStorage(device, pAllocator, storage);
        return false;
    }

    VkMemoryRequirements localMemReqs{};
    VkMemoryRequirements stagingMemReqs{};
    next_->vkGetBufferMemoryRequirements(device, storage.localBuffer, &localMemReqs);
    next_->vkGetBufferMemoryRequirements(device, storage.stagingBuffer, &stagingMemReqs);

    auto localMemoryTypeIndex = GetMemoryTypeIndex(memoryProperties, localMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    auto stagingMemoryTypeIndex = GetMemoryTypeIndex(memoryProperties, localMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (!localMemoryTypeIndex) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not find memory type with VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT\n";
        DestroyShaderProfileStorage(device, pAllocator, storage);
        return false;
    }

    if (!stagingMemoryTypeIndex) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not find memory type with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT\n";
        DestroyShaderProfileStorage(device, pAllocator, storage);
        return false;
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = localMemReqs.size;
    mai.memoryTypeIndex = localMemoryTypeIndex.value();

    vkres = next_->vkAllocateMemory(device, &mai, pAllocator, &storage.localMemory);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not allocate local memory for shader\n";
        DestroyShaderProfileStorage(device, pAllocator, storage);
        return false;
    }

    mai.allocationSize = stagingMemReqs.size;
    mai.memoryTypeIndex = stagingMemoryTypeIndex.value();

    vkres = next_->vkAllocateMemory(device, &mai, pAllocator, &storage.stagingMemory);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not allocate staging memory for shader\n";
        DestroyShaderProfileStorage(device, pAllocator, storage);
        return false;
    }

    vkres = next_->vkBindBufferMemory(device, storage.localBuffer, storage.localMemory, 0);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not bind local buffer to local memory\n";
        DestroyShaderProfileStorage(device, pAllocator, storage);
        return false;
    }

    vkres = next_->vkBindBufferMemory(device, storage.stagingBuffer, storage.stagingMemory, 0);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateShaderProfileStorage: Could not bind staging buffer to staging memory\n";
        DestroyShaderProfileStorage(device, pAllocator, storage);
        return false;
    }

    storageOut = storage;
    return true;
}

void VulkanLayerShaderProfiler::DestroyShaderProfileStorage(VkDevice device, const VkAllocationCallbacks* pAllocator, const ShaderProfileStorage& storage) const
{
    next_->vkFreeMemory(device, storage.localMemory, pAllocator);
    next_->vkFreeMemory(device, storage.stagingMemory, pAllocator);
    next_->vkDestroyBuffer(device, storage.localBuffer, pAllocator);
    next_->vkDestroyBuffer(device, storage.stagingBuffer, pAllocator);
}

bool VulkanLayerShaderProfiler::CreatePipelineProfileCommandInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, PipelineProfileCommandInfo& commandInfoOut) const
{
    auto deviceInfoIt = deviceInfoMap_.find(device);
    if (deviceInfoIt == deviceInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::CreatePipelineProfileCommandInfo: Could not find device info\n";
        return false;
    }

    const auto& deviceInfo = deviceInfoIt->second;

    PipelineProfileCommandInfo commandInfo;
    commandInfo.queue = deviceInfo.transferQueue;

    VkCommandPoolCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = deviceInfo.transferQueueFamilyIndex;

    VkResult vkres = next_->vkCreateCommandPool(device, &cpci, pAllocator, &commandInfo.cmdPool);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreatePipelineProfileCommandInfo: Could not create command pool\n";
        DestroyPipelineProfileCommandInfo(device, pAllocator, commandInfo);
        return false;
    }

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = commandInfo.cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    vkres = next_->vkAllocateCommandBuffers(device, &cbai, &commandInfo.cmdBuf);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreatePipelineProfileCommandInfo: Could not allocate command buffer\n";
        DestroyPipelineProfileCommandInfo(device, pAllocator, commandInfo);
        return false;
    }

    PatchDispatchKey(device, commandInfo.cmdBuf);

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    vkres = next_->vkCreateFence(device, &fci, pAllocator, &commandInfo.fence);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreatePipelineProfileCommandInfo: Could not create fence\n";
        DestroyPipelineProfileCommandInfo(device, pAllocator, commandInfo);
        return false;
    }

    commandInfoOut = commandInfo;
    return true;
}

void VulkanLayerShaderProfiler::DestroyPipelineProfileCommandInfo(VkDevice device, const VkAllocationCallbacks* pAllocator, const PipelineProfileCommandInfo& commandInfo) const
{
    next_->vkDestroyCommandPool(device, commandInfo.cmdPool, pAllocator);
    next_->vkDestroyFence(device, commandInfo.fence, pAllocator);
}

bool VulkanLayerShaderProfiler::CreateProfileDescriptorSetLayout(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<ShaderProfileInfo>& shaderInfos, VkDescriptorSetLayout& setLayoutOut) const
{
    uint32_t stagesCount = shaderInfos.size();
    std::vector<VkDescriptorSetLayoutBinding> bindings(stagesCount);
    for (uint32_t i = 0; i < stagesCount; ++i) {
        const auto& shaderInfo = shaderInfos[i];
        auto& binding = bindings[i];

        binding.binding = shaderInfo.profileBinding;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = GetVulkanShaderStageBit(shaderInfo.stage);
        binding.pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = bindings.size();
    dslci.pBindings = bindings.data();

    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkResult vkres = next_->vkCreateDescriptorSetLayout(device, &dslci, pAllocator, &setLayout);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateProfileDescriptorSetLayout: Could not create profile descriptor set layout\n";
        return false;
    }

    setLayoutOut = setLayout;
    return true;
}

void VulkanLayerShaderProfiler::DestroyProfileDescriptorSetLayout(VkDevice device, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout setLayout) const
{
    next_->vkDestroyDescriptorSetLayout(device, setLayout, pAllocator);
}

bool VulkanLayerShaderProfiler::CreateProfileDescriptorPool(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<ShaderProfileInfo>& shaderInfos, VkDescriptorPool& descriptorPoolOut) const
{
    uint32_t stagesCount = shaderInfos.size();

    VkDescriptorPoolSize descriptorPoolSize{};
    descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorPoolSize.descriptorCount = stagesCount;

    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &descriptorPoolSize;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkResult vkres = next_->vkCreateDescriptorPool(device, &dpci, pAllocator, &descriptorPool);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateProfileDescriptorPool: Could not create profile descriptor pool\n";
        return false;
    }

    descriptorPoolOut = descriptorPool;
    return true;
}

void VulkanLayerShaderProfiler::DestroyProfileDescriptorPool(VkDevice device, const VkAllocationCallbacks* pAllocator, VkDescriptorPool descriptorPool) const
{
    next_->vkDestroyDescriptorPool(device, descriptorPool, pAllocator);
}

bool VulkanLayerShaderProfiler::SetupProfileDescriptorSet(VkDevice device, const std::vector<ShaderProfileInfo>& shaderInfos, VkDescriptorSetLayout setLayout, VkDescriptorPool descriptorPool, VkDescriptorSet& descriptorSetOut) const
{
    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = descriptorPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &setLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult vkres = next_->vkAllocateDescriptorSets(device, &dsai, &descriptorSet);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::SetupProfileDescriptorSet: Could not allocate pipeline profile descriptor set\n";
        return false;
    }

    uint32_t stagesCount = shaderInfos.size();
    std::vector<VkWriteDescriptorSet> descriptorWrites(stagesCount);
    std::vector<VkDescriptorBufferInfo> descriptorBufferInfos(stagesCount);
    for (uint32_t i = 0; i < stagesCount; ++i) {
        const auto& shaderInfo = shaderInfos[i];
        auto& descriptorWrite = descriptorWrites[i];
        auto& descriptorBufferInfo = descriptorBufferInfos[i];

        descriptorBufferInfo.buffer = shaderInfo.storage.localBuffer;
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range = VK_WHOLE_SIZE;

        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = shaderInfo.profileBinding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.pBufferInfo = &descriptorBufferInfo;
    }

    next_->vkUpdateDescriptorSets(device, stagesCount, descriptorWrites.data(), 0, nullptr);

    descriptorSetOut = descriptorSet;
    return true;
}

bool VulkanLayerShaderProfiler::CreateProfilePipelineLayout(VkDevice device, const VkAllocationCallbacks* pAllocator, const std::vector<ShaderProfileInfo>& shaderInfos,
    VkPipelineLayout origLayout, VkDescriptorSetLayout profileSetLayout, VkPipelineLayout& pipelineLayoutOut) const
{
    auto pipelineLayoutInfoIt = pipelineLayoutInfoMap_.find(origLayout);
    if (pipelineLayoutInfoIt == pipelineLayoutInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::CreateProfilePipelineLayout: Could not find pipeline layout info\n";
        return false;
    }

    const auto& pipelineLayoutInfo = pipelineLayoutInfoIt->second;
    const auto& pushConstantRanges = pipelineLayoutInfo.pushConstantRanges;
    auto setLayouts = pipelineLayoutInfo.setLayouts;

    const auto& shaderInfo = shaderInfos[0];
    uint32_t profileSet = shaderInfo.profileSet;
    if (setLayouts.size() <= profileSet) {
        setLayouts.resize(profileSet + 1);
    }
    setLayouts[profileSet] = profileSetLayout;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.pSetLayouts = setLayouts.data();
    plci.setLayoutCount = setLayouts.size();
    plci.pPushConstantRanges = pushConstantRanges.data();
    plci.pushConstantRangeCount = pushConstantRanges.size();

    VkPipelineLayout modifiedLayout = VK_NULL_HANDLE;
    VkResult vkres = next_->vkCreatePipelineLayout(device, &plci, pAllocator, &modifiedLayout);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CreateProfilePipelineLayout: Could not create profile pipeline layout\n";
        return false;
    }

    pipelineLayoutOut = modifiedLayout;
    return true;
}

void VulkanLayerShaderProfiler::DestroyProfilePipelineLayout(VkDevice device, const VkAllocationCallbacks* pAllocator, VkPipelineLayout pipelineLayout) const
{
    next_->vkDestroyPipelineLayout(device, pipelineLayout, pAllocator);
}

bool VulkanLayerShaderProfiler::BindProfilePipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) const
{
    if (!IsPipelineBindPointSupported(pipelineBindPoint)) {
        std::cout << "VulkanLayerShaderProfiler::BindProfilePipeline: Unsupported pipeline profile bind point\n";
        return false;
    }

    auto pipelineProfileInfoIt = pipelineProfileInfoMap_.find(pipeline);
    if (pipelineProfileInfoIt == pipelineProfileInfoMap_.end()) {
        std::cout << "VulkanLayerShaderProfiler::BindProfilePipeline: Could not find profile pipeline info\n";
        return false;
    }

    const auto& pipelineProfileInfo = pipelineProfileInfoIt->second;
    const auto& shaderProfileInfos = pipelineProfileInfo.shaderInfos;
    VkPipeline modifiedPipeline = pipelineProfileInfo.modifiedPipeline;
    VkPipelineLayout modifiedLayout = pipelineProfileInfo.modifiedLayout;
    VkDescriptorSet profileDescriptorSet = pipelineProfileInfo.profileDescriptorSet;

    next_->vkCmdBindPipeline(commandBuffer, pipelineBindPoint, modifiedPipeline);
    if (!shaderProfileInfos.empty()) {
        const auto& shaderProfileInfo = shaderProfileInfos[0];
        uint32_t profileSet = shaderProfileInfo.profileSet;
        next_->vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, modifiedLayout, profileSet, 1, &profileDescriptorSet, 0, nullptr);
    }
    return true;
}

bool VulkanLayerShaderProfiler::CollectProfileData(VkDevice device, const PipelineProfileInfo& pipelineProfileInfo)
{
    CollectedPipelineProfileInfo collectedProfileInfo;
    collectedProfileInfo.bindPoint = pipelineProfileInfo.bindPoint;
    collectedProfileInfo.pipeline = pipelineProfileInfo.origPipeline;

    const auto& commandInfo = pipelineProfileInfo.commandInfo;

    VkCommandBufferBeginInfo cbbi{};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult vkres = next_->vkBeginCommandBuffer(commandInfo.cmdBuf, &cbbi);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CollectProfileData: Could not begin command buffer\n";
        return false;
    }

    for (const auto& shaderInfo : pipelineProfileInfo.shaderInfos) {
        uint32_t bbCount = shaderInfo.shaderBBCount;
        VkBuffer localBuffer = shaderInfo.storage.localBuffer;
        VkBuffer stagingBuffer = shaderInfo.storage.stagingBuffer;

        VkBufferCopy bufferCopy{};
        bufferCopy.size = bbCount * sizeof(uint64_t);
        next_->vkCmdCopyBuffer(commandInfo.cmdBuf, localBuffer, stagingBuffer, 1, &bufferCopy);
    }

    vkres = next_->vkEndCommandBuffer(commandInfo.cmdBuf);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CollectProfileData: Could not end command buffer\n";
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandInfo.cmdBuf;

    vkres = next_->vkQueueSubmit(commandInfo.queue, 1, &submitInfo, commandInfo.fence);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CollectProfileData: Could not submit command buffer\n";
        return false;
    }

    vkres = next_->vkWaitForFences(device, 1, &commandInfo.fence, VK_TRUE, UINT64_MAX);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CollectProfileData: Could not wait for fence\n";
        return false;
    }

    vkres = next_->vkResetFences(device, 1, &commandInfo.fence);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CollectProfileData: Could not reset fence\n";
        return false;
    }

    vkres = next_->vkResetCommandPool(device, commandInfo.cmdPool, 0);
    if (vkres != VK_SUCCESS) {
        std::cout << "VulkanLayerShaderProfiler::CollectProfileData: Could not reset command pool\n";
        return false;
    }

    for (const auto& shaderInfo : pipelineProfileInfo.shaderInfos) {
        CollectedShaderProfileInfo collectedShaderInfo;
        collectedShaderInfo.stage = shaderInfo.stage;
        collectedShaderInfo.shader = shaderInfo.origShader;
        collectedShaderInfo.code = shaderInfo.origCode;

        auto& profileData = collectedShaderInfo.profileData;

        VkDeviceMemory memory = shaderInfo.storage.stagingMemory;
        uint32_t bbCount = shaderInfo.shaderBBCount;

        void* pData = nullptr;
        VkResult vkres = next_->vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &pData);
        if (vkres != VK_SUCCESS) {
            std::cout << "VulkanLayerShaderProfiler::CollectProfileData: Could not map memory for stage \'" << GetVulkanShaderStageName(shaderInfo.stage) << "\'\n";
            continue;
        }

        profileData.resize(bbCount);
        std::memcpy(profileData.data(), pData, bbCount * sizeof(uint64_t));
        next_->vkUnmapMemory(device, memory);

        collectedProfileInfo.shaderInfos.emplace_back(std::move(collectedShaderInfo));
    }

    collectedProfileInfos_.emplace_back(std::move(collectedProfileInfo));
    return true;
}

} // namespace OVS