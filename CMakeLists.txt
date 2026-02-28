cmake_minimum_required(VERSION 3.22)

project(VulkanLayer_OVS_Debug VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LAYER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Layer)
set(GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Generated)
set(UTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Utils)
set(UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/UI)
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External)
set(JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/JSON)

set(INCLUDE_DIRECTORIES
    ${LAYER_DIR}
    ${GENERATED_DIR}
    ${UTILS_DIR}
    ${UI_DIR}
    ${EXTERNAL_DIR}/Vulkan-Headers/include
    ${EXTERNAL_DIR}/stb
    ${EXTERNAL_DIR}/json/single_include
    ${EXTERNAL_DIR}/SPIRV-Tools
    ${EXTERNAL_DIR}/SPIRV-Tools/include
    ${EXTERNAL_DIR}/SPIRV-Tools/source
    ${EXTERNAL_DIR}/SPIRV-Tools/external/spirv-headers/include
    ${CMAKE_BINARY_DIR}/External/SPIRV-Tools
)

include_directories(${INCLUDE_DIRECTORIES})
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

add_subdirectory(${EXTERNAL_DIR}/SPIRV-Tools)

set(OVS_LAYER_NAME VkLayer_OVS_Debug)

set(OVS_LAYER_SOURCES
    ${LAYER_DIR}/MainLayer.cpp
    ${LAYER_DIR}/VulkanLayer.h
    ${LAYER_DIR}/VulkanLayer.cpp
    ${LAYER_DIR}/VulkanLayerManager.h
    ${LAYER_DIR}/VulkanLayerManager.cpp
    ${LAYER_DIR}/VulkanLayerTerminator.h
    ${LAYER_DIR}/VulkanLayerTerminator.cpp
    ${LAYER_DIR}/VulkanLayerScreenshot.h
    ${LAYER_DIR}/VulkanLayerScreenshot.cpp
    ${LAYER_DIR}/VulkanLayerShaderProfiler.h
    ${LAYER_DIR}/VulkanLayerShaderProfiler.cpp

    ${GENERATED_DIR}/VulkanCoreGenerated.h
    ${GENERATED_DIR}/VulkanDispatchTablesGenerated.h
    ${GENERATED_DIR}/VulkanLayerEntryGenerated.cpp
    ${GENERATED_DIR}/VulkanLayerInterfaceGenerated.h
    ${GENERATED_DIR}/VulkanLayerPassThroughGenerated.h
    ${GENERATED_DIR}/VulkanLayerTerminatorBaseGenerated.h
    ${GENERATED_DIR}/VulkanLayerPrinterGenerated.h
    ${GENERATED_DIR}/VulkanLayerAPIDumpGenerated.h
    ${GENERATED_DIR}/VulkanLayerAPITraceGenerated.h

    ${GENERATED_DIR}/SignatureGenerated.h
    ${GENERATED_DIR}/VulkanAPICallIDGenerated.h
    ${GENERATED_DIR}/VulkanAPIStructIDGenerated.h
    ${GENERATED_DIR}/EnumSerializeGenerated.h
    ${GENERATED_DIR}/EnumSerializeGenerated.cpp
    ${GENERATED_DIR}/StructSerializeGenerated.h
    ${GENERATED_DIR}/StructSerializeToStringGenerated.cpp
    ${GENERATED_DIR}/StructSerializeToStreamGenerated.cpp
    ${GENERATED_DIR}/StructDeserializeFromStreamGenerated.cpp
    ${GENERATED_DIR}/SignatureSerializeToStringGenerated.cpp
    ${GENERATED_DIR}/SignatureSerializeToStreamGenerated.cpp
    ${GENERATED_DIR}/SignatureDeserializeFromStreamGenerated.cpp
    ${GENERATED_DIR}/StructDeepCopyGenerated.h
    ${GENERATED_DIR}/StructDeepCopyGenerated.cpp

    ${UTILS_DIR}/Macros.h
    ${UTILS_DIR}/Vulkan.h
    ${UTILS_DIR}/VulkanPlatform.h
    ${UTILS_DIR}/VulkanUtils.h
    ${UTILS_DIR}/VulkanShader.h
    ${UTILS_DIR}/CommonUtils.h
    ${UTILS_DIR}/PlatformUtils.cpp
)

add_library(${OVS_LAYER_NAME} SHARED ${OVS_LAYER_SOURCES})
target_link_libraries(${OVS_LAYER_NAME} PRIVATE SPIRV-Tools SPIRV-Tools-opt)

file(COPY ${JSON_DIR}/${OVS_LAYER_NAME}.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


set(OVS_DEBUG_NAME OVSDebug)

set(OVS_DEBUG_SOURCES
    ${UI_DIR}/MainUI.cpp
    ${UI_DIR}/ShaderProfiler.h
    ${UI_DIR}/ShaderProfiler.cpp

    ${LAYER_DIR}/VulkanLayer.h
    ${LAYER_DIR}/VulkanLayer.cpp

    ${GENERATED_DIR}/VulkanCoreGenerated.h
    ${GENERATED_DIR}/VulkanDispatchTablesGenerated.h

    ${GENERATED_DIR}/SignatureGenerated.h
    ${GENERATED_DIR}/VulkanAPICallIDGenerated.h
    ${GENERATED_DIR}/VulkanAPIStructIDGenerated.h
    ${GENERATED_DIR}/EnumSerializeGenerated.h
    ${GENERATED_DIR}/EnumSerializeGenerated.cpp
    ${GENERATED_DIR}/StructSerializeGenerated.h
    ${GENERATED_DIR}/StructSerializeToStringGenerated.cpp
    ${GENERATED_DIR}/StructSerializeToStreamGenerated.cpp
    ${GENERATED_DIR}/StructDeserializeFromStreamGenerated.cpp
    ${GENERATED_DIR}/SignatureSerializeToStringGenerated.cpp
    ${GENERATED_DIR}/SignatureSerializeToStreamGenerated.cpp
    ${GENERATED_DIR}/SignatureDeserializeFromStreamGenerated.cpp

    ${UTILS_DIR}/Macros.h
    ${UTILS_DIR}/Vulkan.h
    ${UTILS_DIR}/VulkanPlatform.h
    ${UTILS_DIR}/VulkanUtils.h
    ${UTILS_DIR}/CommonUtils.h
    ${UTILS_DIR}/PlatformUtils.cpp
)

add_executable(${OVS_DEBUG_NAME} ${OVS_DEBUG_SOURCES})
target_link_libraries(${OVS_DEBUG_NAME} PRIVATE SPIRV-Tools SPIRV-Tools-opt)
