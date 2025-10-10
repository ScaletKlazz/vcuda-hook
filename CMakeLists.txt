cmake_minimum_required(VERSION 3.15)
project(vcuda-hook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(CUDAToolkit REQUIRED)
message(STATUS "Using CUDA Toolkit: ${CUDAToolkit_INCLUDE_DIR}")
include_directories(${CUDAToolkit_INCLUDE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

# third party
set(SPDLOG_ROOT ${CMAKE_SOURCE_DIR}/third_party/spdlog)
if(EXISTS ${SPDLOG_ROOT}/CMakeLists.txt)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
    set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "Use external fmt library" FORCE)

    add_subdirectory(${SPDLOG_ROOT} ${CMAKE_BINARY_DIR}/spdlog)
    include_directories(${SPDLOG_ROOT}/include)
    message(STATUS "Using spdlog from submodule: ${SPDLOG_ROOT}")
else()
    message(FATAL_ERROR "spdlog submodule not found or missing CMakeLists.txt. Please run: git submodule update --init --recursive")
endif()

add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

set(YAML_CPP_ROOT ${CMAKE_SOURCE_DIR}/third_party/yaml-cpp)
if(EXISTS ${YAML_CPP_ROOT}/CMakeLists.txt)
    set(YAML_CPP_BUILD_TOOLS  OFF CACHE BOOL "Build parse tool" FORCE)
    
    add_subdirectory(${YAML_CPP_ROOT} ${CMAKE_BINARY_DIR}/yaml-cpp)
    include_directories(${YAML_CPP_ROOT}/include)
    message(STATUS "Using yaml-cpp from submodule: ${YAML_CPP_ROOT}")
else()
    message(FATAL_ERROR "yaml-cpp submodule not found or missing CMakeLists.txt. Please run: git submodule update --init --recursive")
endif()


## add source files
file(GLOB HOOK_SOURCES
        "src/cuda/*.cpp"
        "src/nvml/*.cpp"
        "src/hook/*.cpp"
)
file(GLOB CLIENT_SOURCES 
        "src/client/*.cpp"
        "src/device/*.cpp"
)
file(GLOB UTIL_SOURCES "src/util/*.cpp")

# library
add_library(vcuda-hook SHARED ${HOOK_SOURCES})
add_library(client_lib ${CLIENT_SOURCES})
add_library(util_lib ${UTIL_SOURCES})

# set target
set_target_properties(vcuda-hook PROPERTIES
        LINK_FLAGS "-static-libgcc -static-libstdc++ -Wl,--exclude-libs,ALL"
        COMPILE_FLAGS "-fvisibility=hidden -D_GNU_SOURCE "
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

# link
target_link_libraries(vcuda-hook PRIVATE dl client_lib util_lib)

