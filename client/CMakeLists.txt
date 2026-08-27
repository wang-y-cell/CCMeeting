set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt / WebRTC / xrtc 路径（可通过 -D 覆盖）
if(NOT QT_INSTALL_PATH)
    if(DEFINED ENV{Qt6_DIR})
        # 取得Qt根目录的绝对路径,赋值给QT_INSTALL_PATH
        get_filename_component(QT_INSTALL_PATH "$ENV{Qt6_DIR}/../.." ABSOLUTE)
        message(STATUS "[client] QT_INSTALL_PATH from Qt6_DIR: ${QT_INSTALL_PATH}")
    else()
        set(QT_INSTALL_PATH "F:/Qt/6.8.3/msvc2022_64")
        message(STATUS "[client] QT_INSTALL_PATH default: ${QT_INSTALL_PATH}")
    endif()
endif()
list(PREPEND CMAKE_PREFIX_PATH "${QT_INSTALL_PATH}")

set(XRTC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/webrtcSDK" CACHE PATH "xrtc SDK root")
set(WEBRTC_INCLUDE "F:/wy/webrtc/src" CACHE PATH "libwebrtc headers")
set(WEBRTC_LIB "F:/wy/webrtc/src/out/Default/obj" CACHE PATH "libwebrtc libs")
set(Boost_ROOT "F:/wy/boost_install" CACHE PATH "Boost install prefix")

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia)
find_package(Threads REQUIRED)

set(BUILD_XRTC_DEMO OFF CACHE BOOL "" FORCE)
add_subdirectory("${XRTC_ROOT}" "${CMAKE_BINARY_DIR}/xrtc-build" EXCLUDE_FROM_ALL)

file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui"
)

list(SORT CLIENT_SOURCES)

qt_add_executable(CloudMeetingClient
    MANUAL_FINALIZATION
    ${CLIENT_SOURCES}
    resource.qrc
)

target_link_libraries(CloudMeetingClient
    PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Threads::Threads
    spdlog::spdlog
    xrtc
)

target_include_directories(CloudMeetingClient PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/configure"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message/video"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message/text"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/network/messageNet"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/meeting"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/login"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget/main_window"
)

target_compile_definitions(CloudMeetingClient PRIVATE
    # 声明当前是 Windows 平台，这是 WebRTC 源码中用于条件编译的关键宏
    WEBRTC_WIN
    # Windows 的 <windows.h> 头文件中默认定义了 min 和 max 宏，这会与 C++ 标准库（如 std::min）
    # 或 WebRTC 内部的同名函数发生严重冲突。定义此宏可以禁用 Windows 的 min/max 宏。
    NOMINMAX
    # 用于排除 Windows 头文件中不常用的 API（如 Cryptography、DDE、RPC 等），
    # 从而加快编译速度并减少符号冲突
    WIN32_LEAN_AND_MEAN
    # 仅在 Debug 配置下，将 _ITERATOR_DEBUG_LEVEL 设为 0。这会关闭 MSVC 标准库容器的迭代器调试检查
    $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
    # 仅在 Debug 配置下，将 _HAS_ITERATOR_DEBUGGING 设为 0。这会禁用 MSVC 的迭代器调试功能
    $<$<CONFIG:Debug>:_HAS_ITERATOR_DEBUGGING=0>
)

if(MSVC)
    target_compile_options(CloudMeetingClient PRIVATE /utf-8 /bigobj)
    target_compile_definitions(CloudMeetingClient PRIVATE _WIN32_WINNT=0x0A00)
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
            "$<TARGET_FILE_DIR:CloudMeetingClient>/config"
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
