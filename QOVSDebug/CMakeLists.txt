cmake_minimum_required(VERSION 3.16)

project(QOVSDebug VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(OVS_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

set(OVS_LAYER_DIR ${OVS_ROOT_DIR}/Layer)
set(OVS_GENERATED_DIR ${OVS_ROOT_DIR}/Generated)
set(OVS_UTILS_DIR ${OVS_ROOT_DIR}/Utils)
set(OVS_EXTERNAL_DIR ${OVS_ROOT_DIR}/External)

set(UI_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(UI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(UI_UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${UI_UI_DIR})

set(OVS_INCLUDE_DIRECTORIES
    ${OVS_LAYER_DIR}
    ${OVS_GENERATED_DIR}
    ${OVS_UTILS_DIR}
    ${OVS_EXTERNAL_DIR}/Vulkan-Headers/include
)

set(PROJECT_SOURCES
        ${UI_SRC_DIR}/main.cpp
        ${UI_SRC_DIR}/mainwindow.cpp
        ${UI_SRC_DIR}/apitracewindow.cpp
        ${UI_SRC_DIR}/layeritemdelegate.cpp

        ${UI_INCLUDE_DIR}/mainwindow.h
        ${UI_INCLUDE_DIR}/apitracewindow.h
        ${UI_INCLUDE_DIR}/layeritemdelegate.h

        ${UI_UI_DIR}/mainwindow.ui
        ${UI_UI_DIR}/apitracewindow.ui
)

set(OVS_SOURCES
    ${OVS_LAYER_DIR}/VulkanLayer.h
    ${OVS_LAYER_DIR}/VulkanLayer.cpp

    ${OVS_GENERATED_DIR}/VulkanCoreGenerated.h
    ${OVS_GENERATED_DIR}/VulkanDispatchTablesGenerated.h

    ${OVS_GENERATED_DIR}/SignatureGenerated.h
    ${OVS_GENERATED_DIR}/VulkanAPICallIDGenerated.h
    ${OVS_GENERATED_DIR}/VulkanAPIStructIDGenerated.h
    ${OVS_GENERATED_DIR}/EnumSerializeGenerated.h
    ${OVS_GENERATED_DIR}/EnumSerializeGenerated.cpp
    ${OVS_GENERATED_DIR}/StructSerializeGenerated.h
    ${OVS_GENERATED_DIR}/StructSerializeToStringGenerated.cpp
    ${OVS_GENERATED_DIR}/StructSerializeToParamTreeGenerated.cpp
    ${OVS_GENERATED_DIR}/StructSerializeToStreamGenerated.cpp
    ${OVS_GENERATED_DIR}/StructDeserializeFromStreamGenerated.cpp
    ${OVS_GENERATED_DIR}/SignatureSerializeToStringGenerated.cpp
    ${OVS_GENERATED_DIR}/SignatureSerializeToParamTreeGenerated.cpp
    ${OVS_GENERATED_DIR}/SignatureSerializeToStreamGenerated.cpp
    ${OVS_GENERATED_DIR}/SignatureDeserializeFromStreamGenerated.cpp

    ${OVS_UTILS_DIR}/Macros.h
    ${OVS_UTILS_DIR}/Vulkan.h
    ${OVS_UTILS_DIR}/VulkanPlatform.h
    ${OVS_UTILS_DIR}/VulkanUtils.h
    ${OVS_UTILS_DIR}/CommonUtils.h
    ${OVS_UTILS_DIR}/PlatformUtils.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MainOVS
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${OVS_SOURCES}
        ${UI_UI_DIR}/Resources.qrc
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET MainOVS APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(MainOVS SHARED
            ${PROJECT_SOURCES}
            ${OVS_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(MainOVS
            ${PROJECT_SOURCES}
            ${OVS_SOURCES}
        )
    endif()
endif()

target_include_directories(MainOVS PRIVATE ${OVS_INCLUDE_DIRECTORIES} ${UI_INCLUDE_DIR})
target_compile_options(MainOVS PRIVATE -Wa,-mbig-obj)
target_link_libraries(MainOVS PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MainOVS)
endif()
set_target_properties(MainOVS PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MainOVS
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MainOVS)
endif()
