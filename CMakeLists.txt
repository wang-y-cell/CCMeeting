cmake_minimum_required(VERSION 3.16)
project(CloudMeeting VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_CLIENT "Build CloudMeeting Qt client" ON)
option(BUILD_SERVER "Build CloudMeeting server" OFF)

# 至少一个构建选项被选中
if(NOT BUILD_CLIENT AND NOT BUILD_SERVER)
    message(FATAL_ERROR "[message] At least one of BUILD_CLIENT or BUILD_SERVER must be ON.")
endif()

# spdlog：优先使用仓库内 third_party/spdlog 源码，否则 FetchContent 拉取固定版本
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

set(_SPDLOG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog")
if(EXISTS "${_SPDLOG_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS "[message] Using local spdlog: ${_SPDLOG_SOURCE_DIR}")
    add_subdirectory("${_SPDLOG_SOURCE_DIR}" EXCLUDE_FROM_ALL)
else()
    message(STATUS "[message] Local spdlog not found, fetching v1.17.0 via FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.17.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()
unset(_SPDLOG_SOURCE_DIR)

# 设置编译选项
if(MSVC)
    add_compile_options(/wd"4819") #关闭MSVC警告
    add_compile_options(/utf-8) #设置编码
endif()

if(BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(BUILD_SERVER)
    add_subdirectory(server)
endif()
