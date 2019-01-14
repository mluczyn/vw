cmake_minimum_required(VERSION 3.7.0 FATAL_ERROR)
cmake_policy(VERSION 3.7.0)

project(VW)

# Setup GLFW
include_directories(${INCLEXTERN}/GLFW)
find_library(GLFW_LIBRARY NAMES glfw3.lib HINTS "${CMAKE_SOURCE_DIR}/external/lib/glfw")

# Setup Vulkan SDK dependencies
include_directories($ENV{VULKAN_SDK}/Include)
find_library(SHADERC_LIBRARY NAMES shaderc_combined.lib HINTS "$ENV{VULKAN_SDK}/lib")
find_package(Vulkan)
IF(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Failed to find Vulkan")
ENDIF()

# Setup header-only dependencies
include_directories(external/include)

# VW headers
include_directories(vw/include)

# Definitions and flags

#Enable swapchain extension on Windows
add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)

IF(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(/std:c++17)
	#Synchronous error handling for MSVC
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
ELSE()
	add_definitions(-std=c++17)
ENDIF()

add_subdirectory(vw)
add_subdirectory(examples)