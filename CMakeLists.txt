cmake_minimum_required(VERSION 3.19)
project(CCMeeting VERSION 1.0 LANGUAGES CXX)

if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

#-------------------------------------------------
#
# Options
#
#-------------------------------------------------
option(BUILD_CLIENT "Build CloudMeeting Qt client (MSVC + xrtc)" ON)
option(BUILD_SERVER "Build message_server (TCP meeting)" OFF)
option(BUILD_SERVER2 "Build data_server (HTTP auth)" OFF)

# 客户端依赖 xrtc/webrtc 预编译库，Windows 上必须用 MSVC x64（与 webrtcSDK 一致）
if(BUILD_CLIENT AND WIN32 AND NOT MSVC)
    message(FATAL_ERROR
        "客户端必须用 MSVC x64（cl.exe），当前是 "
        "${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID}).\n"
        "请在已加载 vcvars64.bat 的环境中配置，例如:\n"
        "  cmake -S . -B build/client -G Ninja -DCMAKE_BUILD_TYPE=Debug "
        "-DCMAKE_MAKE_PROGRAM=F:/ninja/ninja.exe "
        "-DQT_INSTALL_PATH=F:/Qt/6.8.3/msvc2022_64")
endif()

if(NOT BUILD_CLIENT AND NOT BUILD_SERVER AND NOT BUILD_SERVER2)
    message(FATAL_ERROR
        "[CCMeeting] 请至少打开一项: "
        "-DBUILD_CLIENT=ON / -DBUILD_SERVER=ON / -DBUILD_SERVER2=ON")
endif()

#-------------------------------------------------
#
# Project defaults
#
#-------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 可选：本机已安装的 Boost（有则跳过自动下载）
set(Boost_ROOT "" CACHE PATH "Boost root (optional; servers/xrtc auto-fetch if empty)")

set(CCMEETING_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(CCMEETING_BOOST_URL
    "https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-b2-nodocs.7z"
    CACHE STRING "Boost source archive URL (Asio/Beast header-only; no b2)")

if(MSVC)
    add_compile_options(/wd4819 /utf-8)
endif()

#-------------------------------------------------
#
# 辅助函数
#
#-------------------------------------------------

# 配置阶段自动 git submodule update
function(ccmeeting_init_git_submodules)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.gitmodules")
        return()
    endif()
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        message(WARNING
            "未找到 Git，无法自动拉取 submodule。"
            "请手动执行: git submodule update --init --recursive")
        return()
    endif()
    message(STATUS "[CCMeeting] Updating git submodules...")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE _git_sub_rc)
    if(NOT _git_sub_rc EQUAL 0)
        message(FATAL_ERROR
            "git submodule update failed (code ${_git_sub_rc}). "
            "请确认 Git 能访问远程，或手动执行:\n"
            "  git submodule update --init --recursive")
    endif()
endfunction()

# 下载压缩包并解压到 dest（marker 已存在则跳过）
function(ccmeeting_fetch_archive)
    set(oneValueArgs URL ARCHIVE DEST MARKER)
    cmake_parse_arguments(FETCH "" "${oneValueArgs}" "" ${ARGN})
    if(FETCH_MARKER AND EXISTS "${FETCH_MARKER}")
        return()
    endif()

    message(STATUS "[CCMeeting] Downloading ${FETCH_URL}")
    set(_archive "${CMAKE_BINARY_DIR}/${FETCH_ARCHIVE}")
    file(DOWNLOAD "${FETCH_URL}" "${_archive}"
        SHOW_PROGRESS
        STATUS _dl_status
        TLS_VERIFY ON
        TIMEOUT 1800
        INACTIVITY_TIMEOUT 120)
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        list(GET _dl_status 1 _dl_msg)
        message(FATAL_ERROR
            "[CCMeeting] Failed to download ${FETCH_URL}: ${_dl_msg}")
    endif()

    file(MAKE_DIRECTORY "${FETCH_DEST}")
    file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${FETCH_DEST}")
    file(REMOVE "${_archive}")

    if(FETCH_MARKER AND NOT EXISTS "${FETCH_MARKER}")
        message(FATAL_ERROR
            "[CCMeeting] Archive extracted to ${FETCH_DEST} but missing ${FETCH_MARKER}")
    endif()
endfunction()

# 定位含 boost/version.hpp 的 include 根
function(ccmeeting_locate_boost_inc out_var)
    set(_roots)
    if(Boost_ROOT)
        list(APPEND _roots "${Boost_ROOT}" "${Boost_ROOT}/include")
    endif()
    if(DEFINED ENV{BOOST_ROOT} AND NOT "$ENV{BOOST_ROOT}" STREQUAL "")
        list(APPEND _roots "$ENV{BOOST_ROOT}" "$ENV{BOOST_ROOT}/include")
    endif()
    list(APPEND _roots
        "${CCMEETING_THIRD_PARTY}/boost-1.92.0"
        "${CCMEETING_THIRD_PARTY}/boost_1_92_0"
        "${CCMEETING_THIRD_PARTY}/boost")
    file(GLOB _boost_ver_files "${CCMEETING_THIRD_PARTY}/*/boost/version.hpp")
    foreach(_f IN LISTS _boost_ver_files)
        get_filename_component(_boost_dir "${_f}" DIRECTORY)
        get_filename_component(_root "${_boost_dir}" DIRECTORY)
        list(APPEND _roots "${_root}")
    endforeach()

    foreach(_r IN LISTS _roots)
        if(EXISTS "${_r}/boost/version.hpp")
            set(${out_var} "${_r}" PARENT_SCOPE)
            return()
        endif()
        if(EXISTS "${_r}/include/boost/version.hpp")
            set(${out_var} "${_r}/include" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out_var} "" PARENT_SCOPE)
endfunction()

# 为服务端提供 Boost::headers / Boost::system（Asio/Beast 仅需头文件）
function(ccmeeting_ensure_boost_for_servers)
    if(TARGET Boost::headers AND TARGET Boost::system)
        message(STATUS "[CCMeeting] Boost targets already present")
        return()
    endif()

    # 优先：已安装的 Boost（带 CMake config / 可 find_package）
    if(Boost_ROOT)
        list(PREPEND CMAKE_PREFIX_PATH "${Boost_ROOT}")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    endif()
    find_package(Boost 1.70 QUIET COMPONENTS system)
    if(Boost_FOUND AND TARGET Boost::headers AND TARGET Boost::system)
        message(STATUS "[CCMeeting] Boost (find_package): ${Boost_VERSION}")
        return()
    endif()

    ccmeeting_locate_boost_inc(_boost_inc)
    if(NOT _boost_inc)
        message(STATUS
            "[CCMeeting] Boost headers missing, downloading ${CCMEETING_BOOST_URL}")
        ccmeeting_fetch_archive(
            URL "${CCMEETING_BOOST_URL}"
            ARCHIVE "boost-1.92.0-b2-nodocs.7z"
            DEST "${CCMEETING_THIRD_PARTY}")
        ccmeeting_locate_boost_inc(_boost_inc)
    endif()
    if(NOT _boost_inc)
        message(FATAL_ERROR
            "[CCMeeting] Boost headers not found under ${CCMEETING_THIRD_PARTY}. "
            "Pass -DBoost_ROOT=... or check network / CCMEETING_BOOST_URL.")
    endif()

    if(NOT TARGET Boost::headers)
        add_library(CCMeeting_Boost_headers INTERFACE)
        add_library(Boost::headers ALIAS CCMeeting_Boost_headers)
        target_include_directories(CCMeeting_Boost_headers INTERFACE "${_boost_inc}")
        # MSVC 勿自动链 Boost 二进制（源码包未编 lib）
        target_compile_definitions(CCMeeting_Boost_headers INTERFACE BOOST_ALL_NO_LIB)
    endif()
    if(NOT TARGET Boost::system)
        # System 自 1.69 起 header-only；提供同名目标供服务端链接
        if(TARGET CCMeeting_Boost_headers)
            add_library(Boost::system ALIAS CCMeeting_Boost_headers)
        else()
            add_library(CCMeeting_Boost_system INTERFACE)
            add_library(Boost::system ALIAS CCMeeting_Boost_system)
            target_link_libraries(CCMeeting_Boost_system INTERFACE Boost::headers)
        endif()
    endif()
    message(STATUS "[CCMeeting] Boost headers: ${_boost_inc}")
endfunction()

ccmeeting_init_git_submodules()

#-------------------------------------------------
#
# third_party: spdlog（git submodule，落在源码树）
#
#-------------------------------------------------
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

if(NOT EXISTS "${CCMEETING_THIRD_PARTY}/spdlog/CMakeLists.txt")
    message(FATAL_ERROR
        "spdlog submodule missing: ${CCMEETING_THIRD_PARTY}/spdlog\n"
        "Run: git submodule update --init --recursive")
endif()
message(STATUS "[CCMeeting] spdlog: ${CCMEETING_THIRD_PARTY}/spdlog")
add_subdirectory("${CCMEETING_THIRD_PARTY}/spdlog" EXCLUDE_FROM_ALL)

if(TARGET spdlog AND MSVC)
    # 与 xrtc / 预编译 webrtc.lib 一致
    target_compile_definitions(spdlog PRIVATE
        _ITERATOR_DEBUG_LEVEL=0
        _HAS_ITERATOR_DEBUGGING=0)
endif()

#-------------------------------------------------
#
# Targets
# 客户端先于 json：webrtcSDK 会创建名为 nlohmann_json 的 INTERFACE，
# 若此处先 add_subdirectory(third_party/json) 会重名冲突。
#
#-------------------------------------------------
if(BUILD_CLIENT)
    add_subdirectory(client)
endif()

# 服务端 Boost：本机 find_package → third_party 已有头文件 → 自动下载源码包
if(BUILD_SERVER OR BUILD_SERVER2)
    ccmeeting_ensure_boost_for_servers()
endif()

if(BUILD_SERVER)
    add_subdirectory(message_server)
endif()

#-------------------------------------------------
#
# third_party: nlohmann/json（仅 data_server；优先复用 xrtc 已创建的目标）
#
#-------------------------------------------------
if(BUILD_SERVER2)
    if(NOT TARGET nlohmann_json AND NOT TARGET nlohmann_json::nlohmann_json)
        if(NOT EXISTS "${CCMEETING_THIRD_PARTY}/json/CMakeLists.txt")
            message(FATAL_ERROR
                "nlohmann/json submodule missing: ${CCMEETING_THIRD_PARTY}/json\n"
                "Run: git submodule update --init --recursive")
        endif()
        set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
        set(JSON_Install OFF CACHE BOOL "" FORCE)
        message(STATUS "[CCMeeting] nlohmann/json: ${CCMEETING_THIRD_PARTY}/json")
        add_subdirectory("${CCMEETING_THIRD_PARTY}/json" EXCLUDE_FROM_ALL)
    endif()
    if(TARGET nlohmann_json AND NOT TARGET nlohmann_json::nlohmann_json)
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    endif()
    add_subdirectory(data_server)
endif()

#-------------------------------------------------
#
# Summary
#
#-------------------------------------------------
message(STATUS "[CCMeeting] BUILD_CLIENT=${BUILD_CLIENT} "
               "BUILD_SERVER=${BUILD_SERVER} BUILD_SERVER2=${BUILD_SERVER2}")
if(Boost_ROOT)
    message(STATUS "[CCMeeting] Boost_ROOT=${Boost_ROOT}")
endif()
