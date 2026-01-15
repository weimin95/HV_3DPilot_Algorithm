cmake_minimum_required(VERSION 3.10)
project(HV_3DPilot_Algorithm)

# 1. 公共配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== OpenCV 配置 ====================
set(OpenCV_DIR "D:/proj/3rdParty/opencv410/build")
find_package(OpenCV REQUIRED)

# ==================== Open3D 配置 ====================
set(Open3D_DIR "D:/proj/3rdParty/open3d-devel-windows-amd64-0.17.0/CMake") 
find_package(Open3D REQUIRED)

# ==================== PCL 配置 ====================
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

# 包含接口头文件、OpenCV、Open3D 和 PCL 头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
    ${Open3D_INCLUDE_DIRS}
    ${PCL_INCLUDE_DIRS}
    D:/proj/git/HV_3DPilot-zengxin/HV_3DPliot/header/public
)

# PCL 定义和链接目录
add_definitions(${PCL_DEFINITIONS})
link_directories(${PCL_LIBRARY_DIRS})

# 2. 定义自动化生成 DLL 的宏
macro(add_algorithm_plugin DIR_NAME)
    # 获取该目录下所有的 .cpp 文件
    file(GLOB_RECURSE PLUGIN_SRC "${CMAKE_SOURCE_DIR}/source/${DIR_NAME}/*.cpp")
    
    # 获取该目录下所有的 .h 和 .hpp 文件
    file(GLOB_RECURSE PLUGIN_HEADERS 
        "${CMAKE_SOURCE_DIR}/source/${DIR_NAME}/*.h"
        "${CMAKE_SOURCE_DIR}/source/${DIR_NAME}/*.hpp"
    )
    
    # 获取 include 目录下的公共头文件
    file(GLOB_RECURSE COMMON_HEADERS 
        "${CMAKE_SOURCE_DIR}/include/*.h"
        "${CMAKE_SOURCE_DIR}/include/*.hpp"
    )
    
    if(PLUGIN_SRC)
        message(STATUS "Found Plugin: ${DIR_NAME}")
        
        # 以文件夹名字作为 DLL 目标名称，包含源文件和头文件
        add_library(${DIR_NAME} SHARED 
            ${PLUGIN_SRC} 
            ${PLUGIN_HEADERS}
            ${COMMON_HEADERS}
        )
        
        # 统一设置导出宏
        target_compile_definitions(${DIR_NAME} PRIVATE HV_3DPILOT_EXPORTS)
        
        # 统一输出到 plugins 目录
        set_target_properties(${DIR_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/HV_3DPilot_Algorithm$<CONFIG>/plugins"
        )
        
        # 在 Visual Studio 中为头文件创建过滤器（文件夹）
        source_group("Header Files\\Plugin" FILES ${PLUGIN_HEADERS})
        source_group("Header Files\\Common" FILES ${COMMON_HEADERS})
        source_group("Source Files" FILES ${PLUGIN_SRC})
        
        # 链接库：同时链接 OpenCV、Open3D 和 PCL
        target_link_libraries(${DIR_NAME} PRIVATE 
            ${OpenCV_LIBS} 
            Open3D::Open3D
            ${PCL_LIBRARIES}
        )
    endif()
endmacro()

# 3. 自动遍历 src 目录下的第一级子文件夹
file(GLOB CHILDREN RELATIVE ${CMAKE_SOURCE_DIR}/source ${CMAKE_SOURCE_DIR}/source/*)

foreach(CHILD ${CHILDREN})
    if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/source/${CHILD})
        # 为每个子文件夹调用宏，生成一个 DLL
        add_algorithm_plugin(${CHILD})
    endif()
endforeach()