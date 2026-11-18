# --------------------------------------------------
# 客户端单元测试（Qt Test）
#
# 两种用法：
#
# 1) 主构建（随客户端一起）：
#      cmake -S . -B build/client -DBUILD_TESTS=ON
#      ctest --test-dir build/client --output-on-failure
#
# 2) 独立构建（CI 用，不需要 WebRTC SDK / 不需要子模块）：
#      cmake -S client/tests -B build/tests
#      ctest --test-dir build/tests --output-on-failure
#
# 原则：测试目标只链接「不依赖界面 / 网络 / 媒体引擎」的纯逻辑单元，
# 这样在没有服务器、没有摄像头的机器上也能跑。
# --------------------------------------------------
cmake_minimum_required(VERSION 3.19)

# 测试类自身带 Q_OBJECT（QTEST_*_MAIN 需要 moc）
set(CMAKE_AUTOMOC ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 独立构建：自己解决 Qt 与 spdlog，避免依赖 third_party/webrtcSDK
    project(ccmeeting_client_tests LANGUAGES CXX)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CCMEETING_TESTS_STANDALONE ON)

    find_package(Qt6 REQUIRED COMPONENTS Core Gui Multimedia Test)

    if(NOT TARGET spdlog::spdlog)
        include(FetchContent)
        set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
        set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
        set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.17.0
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(spdlog)
    endif()

    enable_testing()
else()
    # 作为 client/ 的子目录：Qt / spdlog / 编译选项由父工程提供
    find_package(Qt6 REQUIRED COMPONENTS Gui Multimedia Test)
endif()

set(CCMEETING_TEST_LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/protocol/message.cpp"
)

function(ccmeeting_add_test target)
    add_executable(${target} ${ARGN} ${CCMEETING_TEST_LIB_SOURCES})
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    )
    target_link_libraries(${target} PRIVATE
        Qt6::Core
        Qt6::Test
        spdlog::spdlog
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /bigobj)
    endif()
    if(NOT CCMEETING_TESTS_STANDALONE)
        # 必须与主工程的 spdlog 保持一致的迭代器调试级别，
        # 否则 MSVC 会报 LNK2038（_ITERATOR_DEBUG_LEVEL 不匹配）
        target_compile_definitions(${target} PRIVATE
            _ITERATOR_DEBUG_LEVEL=0
            _HAS_ITERATOR_DEBUGGING=0
        )
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# 发送队列：优先级、超时、上限淘汰
ccmeeting_add_test(test_priority_queue
    test_priority_queue.cpp)

# 编解码：线上 JSON 格式与二进制 payload 布局
ccmeeting_add_test(test_message_codec
    test_message_codec.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/protocol/message_codec.cpp")

# 协议分发：网络消息 → 语义信号
# 注意：被测类头文件必须列进源列表，AUTOMOC 才会为它生成 moc（Q_OBJECT）
ccmeeting_add_test(test_protocol_dispatcher
    test_protocol_dispatcher.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/features/meeting/meeting_protocol_dispatcher.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/features/meeting/meeting_protocol_dispatcher.cpp")

# 画面样式预设：头像 72×72、主画面头像 10% 高度、视频等比铺满
ccmeeting_add_test(test_video_style
    test_video_style.cpp)

# 设备目录：偏好键、未知/空设备、无格式时的回落（不枚举真实设备）
ccmeeting_add_test(test_device_catalog
    test_device_catalog.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/media/devices/device_catalog.cpp")
target_link_libraries(test_device_catalog PRIVATE Qt6::Multimedia)
