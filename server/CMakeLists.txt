
set(BOOST_ROOT "" CACHE PATH "Boost 安装路径（包含 include/boost 的目录）")
if(BOOST_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${BOOST_ROOT}")
endif()

find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(SORT SERVER_SOURCES)

add_executable(CloudMeetingServer ${SERVER_SOURCES})

target_include_directories(CloudMeetingServer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
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
