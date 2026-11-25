#-------------------------------------------------
#
# message_server（Boost.Asio TCP）
#
#-------------------------------------------------
# Boost::headers / Boost::system 由根 CMake ccmmeeting_ensure_boost_for_servers 提供
# （本机安装、third_party 头文件或自动下载）；此处仅作兜底。
if(NOT TARGET Boost::headers OR NOT TARGET Boost::system)
    if(Boost_ROOT)
        list(PREPEND CMAKE_PREFIX_PATH "${Boost_ROOT}")
    elseif(BOOST_ROOT)
        list(PREPEND CMAKE_PREFIX_PATH "${BOOST_ROOT}")
        set(Boost_ROOT "${BOOST_ROOT}" CACHE PATH "Boost install prefix" FORCE)
    endif()
    find_package(Boost 1.70 REQUIRED COMPONENTS system)
endif()
find_package(Threads REQUIRED)

# nlohmann/json：使用仓库声明的 third_party/json 子模块（与 message_server/tests 一致），
# 不再借用 xrtc 内部的嵌套副本，避免两份头文件版本/路径漂移。
set(CCMEETING_JSON_INCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/json/include")
if(NOT EXISTS "${CCMEETING_JSON_INCLUDE}/nlohmann/json.hpp")
    message(FATAL_ERROR
        "[message_server] 缺少 json 子模块：${CCMEETING_JSON_INCLUDE}\n"
        "请执行: git submodule update --init third_party/json")
endif()

file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(SORT SERVER_SOURCES)

add_executable(CloudMeetingServer ${SERVER_SOURCES})

target_include_directories(CloudMeetingServer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CCMEETING_JSON_INCLUDE}"
)

target_link_libraries(CloudMeetingServer
    PRIVATE
        Boost::headers
        Boost::system
        Threads::Threads
        spdlog::spdlog
)

if(WIN32)
    target_link_libraries(CloudMeetingServer PRIVATE ws2_32 mswsock)
endif()

set_target_properties(CloudMeetingServer PROPERTIES
    OUTPUT_NAME CloudMeetingServer
)

include(GNUInstallDirs)
install(TARGETS CloudMeetingServer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
