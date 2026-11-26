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
set(CMAKE_AUTORCC ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 独立构建：自己解决 Qt 与 spdlog，避免依赖 third_party/webrtcSDK
    project(ccmeeting_client_tests LANGUAGES CXX)

# CI/新环境没有私有 client.json：用脱敏示例生成（qrc 需要该文件）
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../config/client.json")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../config/client.json.example"
                   "${CMAKE_CURRENT_SOURCE_DIR}/../config/client.json" COPYONLY)
endif()

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CCMEETING_TESTS_STANDALONE ON)

    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia Network Sql WebSockets Svg Test)

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
    find_package(Qt6 REQUIRED COMPONENTS Gui Widgets Multimedia Network Sql WebSockets Svg Test)
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

# 编解码：统一 JSON 信封（上行 t/token/data，下行 t/code/message/data）
ccmeeting_add_test(test_message_codec
    test_message_codec.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/protocol/message_codec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/user_session.cpp")

# 协议分发：网络消息 → 语义信号
# 直接喂下行 JSON（经 MessageCodec 解码），锁定 code/data → 信号值的映射。
# 注意：被测类头文件必须列进源列表，AUTOMOC 才会为它生成 moc（Q_OBJECT）
ccmeeting_add_test(test_protocol_dispatcher
    test_protocol_dispatcher.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/features/meeting/meeting_protocol_dispatcher.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/features/meeting/meeting_protocol_dispatcher.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/protocol/message_codec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/user_session.cpp")

# 画面样式预设：头像 72×72、主画面头像 10% 高度、视频等比铺满
ccmeeting_add_test(test_video_style
    test_video_style.cpp)

# 画面控件（CPU 光栅）：清屏色 / 头像透明角落 / I420 转换系数 / 说话边框，
# 并加一条护栏防止有人把基类换回 QOpenGLWidget（那样会重新引入整窗 GL 合成
# 与缩放闪烁）。xrtc 只有头文件依赖，独立构建时若缺 SDK 就跳过。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/webrtcSDK/include/xrtc/xrtc_defines.h")
    ccmeeting_add_test(test_video_widget
        test_video_widget.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../include/media/video/video_gl_widget.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src/media/video/video_gl_widget.cpp")
    target_include_directories(test_video_widget PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/webrtcSDK/include")
    target_link_libraries(test_video_widget PRIVATE Qt6::Widgets)
endif()

# 连接协调器：挂起/补发/建会入会动作（纯逻辑，无网络）
ccmeeting_add_test(test_connect_coordinator
    test_connect_coordinator.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/features/meeting/connect_coordinator.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/features/meeting/connect_coordinator.cpp")
target_link_libraries(test_connect_coordinator PRIVATE Qt6::Network)

# 本地存储 + 会议记录：临时 SQLite（QStandardPaths 测试模式）
ccmeeting_add_test(test_local_store
    test_local_store.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/storage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/data/meeting_history.cpp")
target_link_libraries(test_local_store PRIVATE Qt6::Sql)

# 存储分级：共享库（偏好/凭据）+ 按账号分库（会议记录/录制），含旧数据迁移
ccmeeting_add_test(test_storage_users
    test_storage_users.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/storage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/data/meeting_history.cpp")
target_link_libraries(test_storage_users PRIVATE Qt6::Sql)

# 主窗口骨架：账号操作（退出登录/注销账号）必须在用户卡里
ccmeeting_add_test(test_main_window_ui
    test_main_window_ui.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/ui/shell/main_window_ui.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/shell/main_window_ui.cpp")
target_link_libraries(test_main_window_ui PRIVATE Qt6::Widgets)
# 主题令牌：颜色有效 / 对比度 / 灰调 / hover 区分度
ccmeeting_add_test(test_theme_tokens
    test_theme_tokens.cpp)
target_include_directories(test_theme_tokens PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(test_theme_tokens PRIVATE Qt6::Gui)
# 客户端配置：默认值 / JSON 解析 / 非法 JSON / 保存后重载
ccmeeting_add_test(test_client_config
    test_client_config.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/client_config.cpp")
# 用户会话：生日→年龄、性别文案、资料更新语义
ccmeeting_add_test(test_user_session
    test_user_session.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/user_session.cpp")
# 聊天控制器：提交/待确认气泡/追加/清空（offscreen）
ccmeeting_add_test(test_chat_controller
    test_chat_controller.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/features/meeting/chat_controller.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/features/meeting/chat_controller.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/ui/composites/chat_message.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/composites/chat_message.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/ui/composites/my_text_edit.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/composites/my_text_edit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/user_session.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/media/avatar_service.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/foundation/auth_http.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/tls_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/client_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/preferences.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/storage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/ui/foundation/theme_manager.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/foundation/theme_manager.cpp")
target_link_libraries(test_chat_controller PRIVATE
    Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::Sql Qt6::Svg)
# 窗口行为开关：默认值/关闭/恢复默认/旧键清理
ccmeeting_add_test(test_window_behavior
    test_window_behavior.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/foundation/window_behavior.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/preferences.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/storage.cpp")
target_link_libraries(test_window_behavior PRIVATE Qt6::Sql)
# 网络可靠性：真实回环 WebSocket（P0 发送回执 + P3 自动重连）
ccmeeting_add_test(test_network_reliability
    test_network_reliability.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/protocol/connection.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/protocol/connection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/tls_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/protocol/message_hub.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/protocol/message_hub.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/protocol/message.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/protocol/message_codec.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/user_session.cpp")
target_link_libraries(test_network_reliability PRIVATE
    Qt6::WebSockets Qt6::Widgets Qt6::Network)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 认证 HTTPS：内置 CA、URL 规则、请求头（含 qrc 资源）
ccmeeting_add_test(test_auth_http
    test_auth_http.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/foundation/auth_http.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/tls_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/client_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/config/user_session.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../resource.qrc")
target_link_libraries(test_auth_http PRIVATE Qt6::Network)
