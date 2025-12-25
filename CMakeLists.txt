cmake_minimum_required(VERSION 3.22)

project(VulkanLayer_OVS_Debug VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LAYER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Layer)
set(GENERATED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Generated)
set(UTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Utils)
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External)
set(JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/JSON)

set(INCLUDE_DIRECTORIES
    ${LAYER_DIR}
    ${GENERATED_DIR}
    ${UTILS_DIR}
    ${EXTERNAL_DIR}/Vulkan-Headers/include
)

include_directories(${INCLUDE_DIRECTORIES})

set(OVS_LAYER_NAME VkLayer_OVS_Debug)

set(OVS_LAYER_SOURCES
    ${LAYER_DIR}/Main.cpp
    ${LAYER_DIR}/VulkanLayerManager.h
    ${LAYER_DIR}/VulkanLayerManager.cpp
    ${LAYER_DIR}/VulkanLayerTerminator.h
    ${LAYER_DIR}/VulkanLayerTerminator.cpp

    ${GENERATED_DIR}/VulkanCoreGenerated.h
    ${GENERATED_DIR}/VulkanDispatchTablesGenerated.h
    ${GENERATED_DIR}/VulkanLayerEntryGenerated.cpp
    ${GENERATED_DIR}/VulkanLayerInterfaceGenerated.h
    ${GENERATED_DIR}/VulkanLayerTerminatorBaseGenerated.h
    ${GENERATED_DIR}/VulkanLayerPrinterGenerated.h

    ${UTILS_DIR}/Macros.h
    ${UTILS_DIR}/Vulkan.h
    ${UTILS_DIR}/VulkanPlatform.h
    ${UTILS_DIR}/VulkanUtils.h
)

add_library(${OVS_LAYER_NAME} SHARED ${OVS_LAYER_SOURCES})

file(COPY ${JSON_DIR}/${OVS_LAYER_NAME}.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
