# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

# Project's name
set(NAME "vkPBR")
project(${NAME})

#set output run directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#define path for resources in program
add_definitions(-DRESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/")

#define path for shaders in program
add_definitions(-DSHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/")

add_definitions(-DCMAKE_GENERATOR_PLATFORM=x64)

# Find all source files
file(GLOB_RECURSE SOURCE_FILES
    "src/*.cpp"
)

file(GLOB_RECURSE HEADER_FILES
    "include/*.h"
    "include/*.hpp"
)

# Use FindVulkan module added with CMAKE 3.7
if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
	message(STATUS "Using module to find Vulkan")
    find_package(Vulkan)
endif()


# Windows
IF(WIN32)
    #set libs path
    set(GLFW_LIBRARY ${CMAKE_SOURCE_DIR}/externals/glfw/ CACHE STRING "" FORCE)

    #set libs
    set(LIBS glfw3.lib)

    #include linker paths
    link_directories(${GLFW_LIBRARY})

	#set executables
	add_executable(${NAME} ${SOURCE_FILES} ${HEADER_FILES})

    #link libraries
	target_link_libraries(${NAME} ${LIBS} ${Vulkan_LIBRARY})

    #externals
    target_include_directories( ${NAME} PUBLIC
        include
        externals
        externals/glm
        externals/gli
        externals/glfw
        ${Vulkan_INCLUDE_DIRS}
    )

# Linux
ELSE(WIN32)

ENDIF(WIN32)

# C++ standard
set_property(TARGET ${NAME} PROPERTY CXX_STANDARD 17)

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
if(CMAKE_COMPILER_IS_GNUCXX)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra -Wundef")
endif(CMAKE_COMPILER_IS_GNUCXX)

set_target_properties(${NAME} PROPERTIES LINKER_LANGUAGE CXX)