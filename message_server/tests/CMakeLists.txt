# --------------------------------------------------
# message_server 单元测试（GoogleTest）
#
# 独立构建（不需要 Boost / WebSocket，只测协议编解码与缓冲区）：
#   cmake -S message_server/tests -B build/server-tests -G Ninja
#   cmake --build build/server-tests
#   ctest --test-dir build/server-tests --output-on-failure
# --------------------------------------------------
cmake_minimum_required(VERSION 3.19)
project(ccmeeting_server_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# GoogleTest 走子模块（third_party/gtest），不依赖系统安装
set(GTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gtest")
if(NOT EXISTS "${GTEST_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "[server-tests] 缺少 gtest 子模块：${GTEST_ROOT}\n"
        "请执行: git submodule update --init third_party/gtest")
endif()
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${GTEST_ROOT}" "${CMAKE_BINARY_DIR}/gtest" EXCLUDE_FROM_ALL)
include(GoogleTest)

# nlohmann/json 走子模块（third_party/json）
set(JSON_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/json/include")
if(NOT EXISTS "${JSON_INCLUDE}/nlohmann/json.hpp")
    message(FATAL_ERROR
        "[server-tests] 缺少 json 子模块：${JSON_INCLUDE}\n"
        "请执行: git submodule update --init third_party/json")
endif()

set(SERVER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_executable(test_message_server_codec
    test_codec.cpp
    "${SERVER_ROOT}/src/protocol/codec.cpp"
    "${SERVER_ROOT}/src/network/buffer.cpp"
)

target_include_directories(test_message_server_codec PRIVATE
    "${SERVER_ROOT}/include"
    "${JSON_INCLUDE}"
)

target_link_libraries(test_message_server_codec PRIVATE GTest::gtest_main)

if(MSVC)
    target_compile_options(test_message_server_codec PRIVATE /utf-8 /bigobj)
endif()

gtest_discover_tests(test_message_server_codec
    DISCOVERY_TIMEOUT 30
)
# ---------------- Boost（Asio/Beast 头文件）----------------
# 优先用本机已有头文件：xrtc 子模块里自带了一份 boost-1.92.0
set(BOOST_HINTS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/webrtcSDK/third_party/boost-1.92.0"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/boost-1.92.0"
    "$ENV{BOOST_ROOT}"
    "$ENV{BOOST_ROOT}/include")
find_path(BOOST_INCLUDE_DIR boost/version.hpp HINTS ${BOOST_HINTS})
if(NOT BOOST_INCLUDE_DIR)
    message(FATAL_ERROR "[server-tests] 找不到 Boost 头文件（boost/version.hpp）")
endif()
message(STATUS "[server-tests] Boost: ${BOOST_INCLUDE_DIR}")

# ---------------- spdlog（room/connection 里用）----------------
set(SPDLOG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/spdlog")
if(NOT EXISTS "${SPDLOG_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "[server-tests] 缺少 spdlog 子模块：${SPDLOG_ROOT}")
endif()
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${SPDLOG_ROOT}" "${CMAKE_BINARY_DIR}/spdlog" EXCLUDE_FROM_ALL)
find_package(Threads REQUIRED)

add_executable(test_message_server_room
    test_room.cpp
    "${SERVER_ROOT}/src/meeting/room.cpp"
    "${SERVER_ROOT}/src/meeting/room_manager.cpp"
    "${SERVER_ROOT}/src/network/connection.cpp"
    "${SERVER_ROOT}/src/network/buffer.cpp"
    "${SERVER_ROOT}/src/protocol/codec.cpp"
)
target_include_directories(test_message_server_room PRIVATE
    "${SERVER_ROOT}/include"
    "${JSON_INCLUDE}"
    "${BOOST_INCLUDE_DIR}"
)
target_link_libraries(test_message_server_room PRIVATE
    GTest::gtest_main
    spdlog::spdlog
    Threads::Threads
)
if(WIN32)
    target_link_libraries(test_message_server_room PRIVATE ws2_32 mswsock)
endif()
if(MSVC)
    target_compile_options(test_message_server_room PRIVATE /utf-8 /bigobj)
endif()

gtest_discover_tests(test_message_server_room
    DISCOVERY_TIMEOUT 30
)
# 端到端：真实 loopback WebSocket（Server + MeetingService + Room）
add_executable(test_message_server_service
    test_meeting_service.cpp
    "${SERVER_ROOT}/src/service/meeting_service.cpp"
    "${SERVER_ROOT}/src/network/server.cpp"
    "${SERVER_ROOT}/src/network/connection.cpp"
    "${SERVER_ROOT}/src/network/buffer.cpp"
    "${SERVER_ROOT}/src/meeting/room.cpp"
    "${SERVER_ROOT}/src/meeting/room_manager.cpp"
    "${SERVER_ROOT}/src/protocol/codec.cpp"
    "${SERVER_ROOT}/src/protocol/token.cpp"
)
target_include_directories(test_message_server_service PRIVATE
    "${SERVER_ROOT}/include"
    "${JSON_INCLUDE}"
    "${BOOST_INCLUDE_DIR}"
)
target_link_libraries(test_message_server_service PRIVATE
    GTest::gtest_main
    spdlog::spdlog
    Threads::Threads
)
if(WIN32)
    target_link_libraries(test_message_server_service PRIVATE ws2_32 mswsock)
endif()
if(MSVC)
    target_compile_options(test_message_server_service PRIVATE /utf-8 /bigobj)
endif()

gtest_discover_tests(test_message_server_service
    DISCOVERY_TIMEOUT 60
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
