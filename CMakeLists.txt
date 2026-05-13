cmake_minimum_required(VERSION 3.10)
project(HV_3DPilot_Algorithm)

# Common configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(HV_3DPILOT_ALGORITHM_BUILD_TESTS "Build algorithm plugin tests" OFF)

# OpenCV
set(OpenCV_DIR "D:/proj/3rdParty/opencv4.10_with_contrib/opencv-4.10.0/build/install")
find_package(OpenCV REQUIRED)

# Open3D
set(Open3D_DIR "D:/proj/3rdParty/open3d-devel-windows-amd64-0.17.0/CMake")
find_package(Open3D REQUIRED)

# PCL
set(PCL_DIR "D:/proj/3rdParty/PCL/cmake")
find_package(PCL REQUIRED COMPONENTS
    common
    io
    filters
    segmentation
    search
    kdtree
    features
    surface
    visualization
    sample_consensus
    octree
    registration
    keypoints
    tracking
    recognition
    ml
    people
    outofcore
    stereo
)

# Include directories
set(HV_3DPILOT_PUBLIC_INCLUDE_DIR
    "D:/proj/git/HV_3DPilot-zengxin/HV_3DPliot/header/public"
    CACHE PATH
    "Path to HV_3DPilot public headers")

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${Open3D_INCLUDE_DIRS}
    ${PCL_INCLUDE_DIRS}
    ${HV_3DPILOT_PUBLIC_INCLUDE_DIR}
)

# PCL definitions and link directories
add_definitions(${PCL_DEFINITIONS})
link_directories(${PCL_LIBRARY_DIRS})

# Build common static library
file(GLOB_RECURSE COMMON_SRC "${CMAKE_SOURCE_DIR}/source/common/*.cpp")
list(APPEND COMMON_SRC "${CMAKE_SOURCE_DIR}/third_party/tinyexpr/tinyexpr.cpp")
file(GLOB_RECURSE COMMON_HEADERS
    "${CMAKE_SOURCE_DIR}/source/common/*.h"
    "${CMAKE_SOURCE_DIR}/source/common/*.hpp"
)

if(COMMON_SRC)
    message(STATUS "Building Common Library with ${COMMON_SRC}")
    add_library(common_lib STATIC ${COMMON_SRC} ${COMMON_HEADERS})

    if(MSVC)
        target_compile_options(common_lib PRIVATE /source-charset:utf-8)
    endif()

    target_link_libraries(common_lib PUBLIC
        ${OpenCV_LIBS}
        Open3D::Open3D
        ${PCL_LIBRARIES}
    )

    target_include_directories(common_lib PUBLIC
        ${CMAKE_SOURCE_DIR}/source/common
        ${CMAKE_SOURCE_DIR}/third_party/tinyexpr
    )

    source_group("Header Files" FILES ${COMMON_HEADERS})
    source_group("Source Files" FILES ${COMMON_SRC})
endif()

add_library(lua_vendor STATIC
    ${CMAKE_SOURCE_DIR}/third_party/lua/onelua.c
)

target_compile_definitions(lua_vendor PRIVATE
    MAKE_LIB
    _CRT_SECURE_NO_WARNINGS
)

target_include_directories(lua_vendor PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/lua
)

# Build one plugin from source/<Category>/<PluginName>
macro(add_algorithm_plugin CATEGORY_NAME PLUGIN_NAME)
    set(PLUGIN_DIR "${CMAKE_SOURCE_DIR}/source/${CATEGORY_NAME}/${PLUGIN_NAME}")

    file(GLOB_RECURSE PLUGIN_SRC "${PLUGIN_DIR}/*.cpp")
    file(GLOB_RECURSE PLUGIN_HEADERS
        "${PLUGIN_DIR}/*.h"
        "${PLUGIN_DIR}/*.hpp"
    )

    file(GLOB_RECURSE PUBLIC_HEADERS
        "${CMAKE_SOURCE_DIR}/include/*.h"
        "${CMAKE_SOURCE_DIR}/include/*.hpp"
    )

    if(PLUGIN_SRC)
        message(STATUS "Found Plugin: ${CATEGORY_NAME}/${PLUGIN_NAME}")

        add_library(${PLUGIN_NAME} SHARED
            ${PLUGIN_SRC}
            ${PLUGIN_HEADERS}
            ${PUBLIC_HEADERS}
        )

        target_compile_definitions(${PLUGIN_NAME} PRIVATE HV_3DPILOT_EXPORTS)
        if(MSVC)
            target_compile_options(${PLUGIN_NAME} PRIVATE /bigobj)
        endif()

        if(MSVC AND (${PLUGIN_NAME} STREQUAL "HVImageEdge" OR ${PLUGIN_NAME} STREQUAL "HVImageRoiCrop" OR ${PLUGIN_NAME} STREQUAL "HVRoiCreate" OR ${PLUGIN_NAME} STREQUAL "HVImageSave" OR ${PLUGIN_NAME} STREQUAL "HVImageTextDisplay" OR ${PLUGIN_NAME} STREQUAL "ConditionalBranch" OR ${PLUGIN_NAME} STREQUAL "HVSendData" OR ${PLUGIN_NAME} STREQUAL "Format" OR ${PLUGIN_NAME} STREQUAL "ConditionCheck" OR ${PLUGIN_NAME} STREQUAL "VariableCalculate" OR ${PLUGIN_NAME} STREQUAL "HandEyeCalibrationNode"))
            # 新插件源码保存为 UTF-8；执行字符集沿用当前工程默认，保持与现有插件中文返回值一致。
            target_compile_options(${PLUGIN_NAME} PRIVATE /source-charset:utf-8)
        endif()

        set_target_properties(${PLUGIN_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/HV_3DPilot_Algorithm$<CONFIG>/plugins/${CATEGORY_NAME}/${PLUGIN_NAME}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/HV_3DPilot_AlgorithmDebug/plugins/${CATEGORY_NAME}/${PLUGIN_NAME}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/HV_3DPilot_AlgorithmRelease/plugins/${CATEGORY_NAME}/${PLUGIN_NAME}"
        )

        source_group("Header Files\\Plugin" FILES ${PLUGIN_HEADERS})
        source_group("Header Files" FILES ${PUBLIC_HEADERS})
        source_group("Source Files" FILES ${PLUGIN_SRC})

        target_link_libraries(${PLUGIN_NAME} PRIVATE
            ${OpenCV_LIBS}
            Open3D::Open3D
            ${PCL_LIBRARIES}
        )

        if(${PLUGIN_NAME} STREQUAL "HVLuaScriptNode")
            target_include_directories(${PLUGIN_NAME} PRIVATE
                ${CMAKE_SOURCE_DIR}/third_party/lua
                ${CMAKE_SOURCE_DIR}/third_party/sol2/include
            )
            target_link_libraries(${PLUGIN_NAME} PRIVATE lua_vendor)
        endif()

        if(TARGET common_lib)
            target_link_libraries(${PLUGIN_NAME} PRIVATE common_lib)
        endif()

        if(${PLUGIN_NAME} STREQUAL "HandEyeCalibrationNode")
            add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/HandEyeCalibration/HandEyeCalibration.dll"
                    "$<TARGET_FILE_DIR:${PLUGIN_NAME}>/HandEyeCalibration.dll"
                COMMENT "Deploy HandEyeCalibration.dll"
            )
        endif()
    endif()
endmacro()

# Discover plugins in source/<Category>/<PluginName>
file(GLOB CATEGORY_DIRS RELATIVE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/source/*)

foreach(CATEGORY_NAME ${CATEGORY_DIRS})
    if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/source/${CATEGORY_NAME} AND NOT ${CATEGORY_NAME} STREQUAL "common")
        file(GLOB PLUGIN_DIRS RELATIVE ${CMAKE_SOURCE_DIR}/source/${CATEGORY_NAME} ${CMAKE_SOURCE_DIR}/source/${CATEGORY_NAME}/*)
        foreach(PLUGIN_NAME ${PLUGIN_DIRS})
            if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/source/${CATEGORY_NAME}/${PLUGIN_NAME})
                add_algorithm_plugin(${CATEGORY_NAME} ${PLUGIN_NAME})
            endif()
        endforeach()
    endif()
endforeach()

# Copy icon resources to runtime plugin root: <plugin_root>/icons
set(ICON_SOURCE_ROOT "${CMAKE_SOURCE_DIR}/resources/icons")
if(EXISTS "${ICON_SOURCE_ROOT}")
    add_custom_target(copy_plugin_icons ALL
        COMMAND ${CMAKE_COMMAND}
            -DICON_SRC_ROOT="${ICON_SOURCE_ROOT}"
            -DICON_DST_ROOT="${CMAKE_BINARY_DIR}/HV_3DPilot_Algorithm$<CONFIG>/plugins/icons"
            -P "${CMAKE_SOURCE_DIR}/cmake/CopyIcons.cmake"
        COMMENT "Copy plugin icon resources to runtime plugins/icons"
    )
endif()

# Deploy OpenCV runtime DLL so plugins can resolve dependencies at load time
set(OPENCV_RUNTIME_BIN "${OpenCV_DIR}/x64/vc17/bin")
add_custom_target(deploy_opencv_runtime ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENCV_RUNTIME_BIN}/opencv_world4100_contrib.dll"
        "${CMAKE_BINARY_DIR}/HV_3DPilot_Algorithm$<CONFIG>/plugins/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OPENCV_RUNTIME_BIN}/opencv_videoio_ffmpeg4100_64.dll"
        "${CMAKE_BINARY_DIR}/HV_3DPilot_Algorithm$<CONFIG>/plugins/"
    COMMENT "Deploy OpenCV runtime DLLs to plugin root"
)

if(HV_3DPILOT_ALGORITHM_BUILD_TESTS)
    add_executable(HVImageReaderDepthTest
        ${CMAKE_SOURCE_DIR}/tests/HVImageReaderDepthTest.cpp
        ${CMAKE_SOURCE_DIR}/source/Capture/HVImageReader/HVImageReader.cpp
    )

    if(MSVC)
        target_compile_options(HVImageReaderDepthTest PRIVATE /source-charset:utf-8)
    endif()

    target_link_libraries(HVImageReaderDepthTest PRIVATE
        ${OpenCV_LIBS}
        Open3D::Open3D
        ${PCL_LIBRARIES}
    )

    if(TARGET common_lib)
        target_link_libraries(HVImageReaderDepthTest PRIVATE common_lib)
    endif()

    add_executable(HVSchemaJsonTraitsTest
        ${CMAKE_SOURCE_DIR}/tests/HVSchemaJsonTraitsTest.cpp
    )

    if(MSVC)
        target_compile_options(HVSchemaJsonTraitsTest PRIVATE /source-charset:utf-8)
    endif()

    if(TARGET common_lib)
        target_link_libraries(HVSchemaJsonTraitsTest PRIVATE common_lib)
    endif()

    add_executable(HVVariableCalculateTest
        ${CMAKE_SOURCE_DIR}/tests/HVVariableCalculateTest.cpp
        ${CMAKE_SOURCE_DIR}/source/LogicTool/VariableCalculate/HVVariableCalculate.cpp
    )

    if(MSVC)
        target_compile_options(HVVariableCalculateTest PRIVATE /source-charset:utf-8)
    endif()

    if(TARGET common_lib)
        target_link_libraries(HVVariableCalculateTest PRIVATE common_lib)
    endif()

    add_executable(HVHandEyeCalibrationTest
        ${CMAKE_SOURCE_DIR}/tests/HVHandEyeCalibrationTest.cpp
        ${CMAKE_SOURCE_DIR}/source/Calibration/HandEyeCalibrationNode/HVHandEyeCalibration.cpp
    )

    if(MSVC)
        target_compile_options(HVHandEyeCalibrationTest PRIVATE /source-charset:utf-8)
    endif()

    target_link_libraries(HVHandEyeCalibrationTest PRIVATE
        ${OpenCV_LIBS}
    )

    if(TARGET common_lib)
        target_link_libraries(HVHandEyeCalibrationTest PRIVATE common_lib)
    endif()
endif()
