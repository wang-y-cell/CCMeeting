#-------------------------------------------------
#
# CloudMeeting client（Qt6 + xrtc）
#
#-------------------------------------------------
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- Qt ----------
set(QT_INSTALL_PATH "F:/Qt/6.8.3/msvc2022_64" CACHE PATH
    "Qt6 install prefix (msvc2022_64)")
if(DEFINED ENV{Qt6_DIR} AND QT_INSTALL_PATH STREQUAL "F:/Qt/6.8.3/msvc2022_64")
    get_filename_component(QT_INSTALL_PATH "$ENV{Qt6_DIR}/../.." ABSOLUTE)
endif()
if(NOT EXISTS "${QT_INSTALL_PATH}")
    message(FATAL_ERROR
        "[client] Qt 路径不存在: ${QT_INSTALL_PATH}\n"
        "请传入 -DQT_INSTALL_PATH=F:/Qt/6.8.3/msvc2022_64类似的路径")
endif()
message(STATUS "[client] QT_INSTALL_PATH=${QT_INSTALL_PATH}")
list(PREPEND CMAKE_PREFIX_PATH "${QT_INSTALL_PATH}")

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia OpenGLWidgets OpenGL Sql WebSockets)
find_package(Threads REQUIRED)

# ---------- xrtc（third_party/webrtcSDK，自包含 WebRTC/json/Boost）----------
set(XRTC_ROOT "${CMAKE_SOURCE_DIR}/third_party/webrtcSDK" CACHE PATH "xrtc SDK root")
if(NOT EXISTS "${XRTC_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "[client] 缺少 xrtc: ${XRTC_ROOT}\n"
        "请执行: git submodule update --init --recursive")
endif()

# 可选：本机 Boost 头文件，跳过 xrtc 内下载
if(NOT Boost_ROOT)
    if(EXISTS "F:/wy/boost_install/boost/version.hpp")
        set(Boost_ROOT "F:/wy/boost_install" CACHE PATH
            "Optional Boost headers for xrtc" FORCE)
    elseif(EXISTS "F:/wy/boost_install/include/boost/version.hpp")
        set(Boost_ROOT "F:/wy/boost_install" CACHE PATH
            "Optional Boost headers for xrtc" FORCE)
    endif()
endif()

set(BUILD_XRTC_DEMO OFF CACHE BOOL "" FORCE)
set(BUILD_XRTC_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${XRTC_ROOT}" "${CMAKE_BINARY_DIR}/xrtc-build" EXCLUDE_FROM_ALL)

# ---------- sources ----------
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(SORT CLIENT_SOURCES)

qt_add_executable(CloudMeetingClient
    MANUAL_FINALIZATION
    ${CLIENT_SOURCES}
    resource.qrc
)

target_include_directories(CloudMeetingClient PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/configure"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message/video"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message/text"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/network"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/network/messageNet"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/meeting"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/login"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/main_window"
)

target_link_libraries(CloudMeetingClient
    PRIVATE
        Qt6::Sql
        Qt6::Widgets
        Qt6::WebSockets
        Qt6::Network
        Qt6::Multimedia
        Qt6::OpenGLWidgets
        Qt6::OpenGL
        Threads::Threads
        spdlog::spdlog
        xrtc
)

# 添加编译期宏
target_compile_definitions(CloudMeetingClient PRIVATE
    WEBRTC_WIN # 定义了 WEBRTC_WIN 宏，表示在 Windows 平台上使用 WebRTC
    NOMINMAX # 定义了 NOMINMAX 宏，表示使用最小化最大值和最小化最小值
    WIN32_LEAN_AND_MEAN # 定义了 WIN32_LEAN_AND_MEAN 宏，表示使用最小化 Windows 头文件
    _ITERATOR_DEBUG_LEVEL=0 # 定义了 _ITERATOR_DEBUG_LEVEL 宏，表示使用迭代器调试级别
    _HAS_ITERATOR_DEBUGGING=0 # 定义了 _HAS_ITERATOR_DEBUGGING 宏，表示使用迭代器调试
)

if(MSVC)
    target_compile_options(CloudMeetingClient PRIVATE /utf-8 /bigobj /wd4068)
    target_compile_definitions(CloudMeetingClient PRIVATE _WIN32_WINNT=0x0A00)
endif()

# 崩溃栈记录（main.cpp 的 SEH 过滤器用 DbgHelp 走栈）
if(WIN32)
    # LoginStore 用 DPAPI 加密本地保存的登录密码
    target_link_libraries(CloudMeetingClient PRIVATE crypt32)
    target_link_libraries(CloudMeetingClient PRIVATE dbghelp)
endif()

if(${Qt6_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.yourcompany.CloudMeeting)
endif()
set_target_properties(CloudMeetingClient PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME CloudMeeting
)

add_custom_command(TARGET CloudMeetingClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:CloudMeetingClient>/config" # 创建config目录
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config/client.json"
            "$<TARGET_FILE_DIR:CloudMeetingClient>/config/client.json"
    COMMENT "Copy client.json to output directory"
)

include(GNUInstallDirs)
install(TARGETS CloudMeetingClient
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(CloudMeetingClient)

#-------------------------------------------------
# UI 预览工具（离线渲染各窗口截图，用于设计评审）
#
#   cmake ... -DBUILD_UI_PREVIEW=ON
#   UiPreview.exe <输出目录>
#
# 与客户端共享全部源码与资源，唯一区别是入口为 tools/ui_preview.cpp：
# 不联网、不登录，逐窗口 grab() 成 PNG。
#-------------------------------------------------
option(BUILD_UI_PREVIEW "Build offline UI preview tool (renders windows to PNG)" OFF)
if(BUILD_UI_PREVIEW)
    set(UIPREVIEW_SOURCES ${CLIENT_SOURCES})
    list(FILTER UIPREVIEW_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")
    list(APPEND UIPREVIEW_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/ui_preview.cpp")

    add_executable(UiPreview ${UIPREVIEW_SOURCES} resource.qrc)

    target_include_directories(UiPreview PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/configure"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/message"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/message/video"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/message/text"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/network"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/network/messageNet"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/widget"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/ui"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/meeting"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/login"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/main_window"
    )

    target_link_libraries(UiPreview
        PRIVATE
            Qt6::Widgets
            Qt6::WebSockets
            Qt6::Sql
            Qt6::Network
            Qt6::Multimedia
            Qt6::OpenGLWidgets
            Qt6::OpenGL
            Threads::Threads
            spdlog::spdlog
            xrtc
    )

    target_compile_definitions(UiPreview PRIVATE
        WEBRTC_WIN
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _ITERATOR_DEBUG_LEVEL=0
        _HAS_ITERATOR_DEBUGGING=0
    )

    if(MSVC)
        target_compile_options(UiPreview PRIVATE /utf-8 /bigobj /wd4068)
        target_compile_definitions(UiPreview PRIVATE _WIN32_WINNT=0x0A00)
    endif()

    set_target_properties(UiPreview PROPERTIES OUTPUT_NAME UiPreview)

    add_custom_command(TARGET UiPreview POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:UiPreview>/config"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/config/client.json"
                "$<TARGET_FILE_DIR:UiPreview>/config/client.json"
        COMMENT "Copy client.json to UiPreview output directory"
    )
endif()
