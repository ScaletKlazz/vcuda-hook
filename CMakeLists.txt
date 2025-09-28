cmake_minimum_required(VERSION 3.15)
project(vcuda-hook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(CUDAToolkit REQUIRED)
include_directories(/usr/local/cuda-12.9/targets/x86_64-linux/include/)
include_directories(${CMAKE_SOURCE_DIR}/include)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_SHALLOW TRUE
    GIT_TAG        v1.x
)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
FetchContent_MakeAvailable(spdlog)

message(${spdlog_SOURCE_DIR}/include)
include_directories(${spdlog_SOURCE_DIR}/include)

add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

## add source files
file(GLOB CUDA_SOURCES "src/cuda/*.cpp")
file(GLOB NVML_SOURCES "src/nvml/*.cpp")
file(GLOB CLIENT_SOURCES "src/client/*.cpp")
file(GLOB UTIL_SOURCES "src/util/*.cpp")

add_library(vcuda-hook SHARED ${CUDA_SOURCES} ${NVML_SOURCES})
add_library(client_lib ${CLIENT_SOURCES})
add_library(util_lib ${UTIL_SOURCES})

set_target_properties(vcuda-hook PROPERTIES
        LINK_FLAGS "-static-libgcc -static-libstdc++ -Wl,--exclude-libs,ALL"
        COMPILE_FLAGS "-fvisibility=hidden -D_GNU_SOURCE "
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/output
)

target_link_libraries(vcuda-hook PRIVATE dl client_lib util_lib)