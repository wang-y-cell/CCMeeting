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
    "${SERVER_ROOT}/src/protocol/message_type.cpp"
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
