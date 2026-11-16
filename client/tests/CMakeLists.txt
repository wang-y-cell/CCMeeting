# --------------------------------------------------
# 客户端单元测试（Qt Test）
#
# 运行：
#   ctest --test-dir build/client --output-on-failure
#
# 原则：测试目标只链接「不依赖界面 / 网络 / 媒体引擎」的纯逻辑单元，
# 这样在没有服务器、没有摄像头的机器上也能跑。
# --------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Test)

set(CCMEETING_TEST_LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/network/messageNet/message.cpp"
)

function(ccmeeting_add_test target)
    add_executable(${target} ${ARGN} ${CCMEETING_TEST_LIB_SOURCES})
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/network"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/network/messageNet"
    )
    target_link_libraries(${target} PRIVATE
        Qt6::Core
        Qt6::Test
        spdlog::spdlog
    )
    # 必须与 spdlog / 客户端主程序保持一致的迭代器调试级别，
    # 否则 MSVC 会报 LNK2038（_ITERATOR_DEBUG_LEVEL 不匹配）
    target_compile_definitions(${target} PRIVATE
        _ITERATOR_DEBUG_LEVEL=0
        _HAS_ITERATOR_DEBUGGING=0
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# 发送队列：优先级、超时、上限淘汰
ccmeeting_add_test(test_priority_queue
    test_priority_queue.cpp)

# 编解码：线上 JSON 格式与二进制 payload 布局
ccmeeting_add_test(test_message_codec
    test_message_codec.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/network/messageNet/messagecodec.cpp")

# 协议分发：网络消息 → 语义信号
# 注意：被测类头文件必须列进源列表，AUTOMOC 才会为它生成 moc（Q_OBJECT）
ccmeeting_add_test(test_protocol_dispatcher
    test_protocol_dispatcher.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/features/meeting/meeting_protocol_dispatcher.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/features/meeting/meeting_protocol_dispatcher.cpp")
