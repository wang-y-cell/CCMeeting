set(BOOST_ROOT "" CACHE PATH "Boost 安装路径（包含 include/boost 的目录）")
if(BOOST_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${BOOST_ROOT}")
endif()

find_package(Boost 1.70 REQUIRED COMPONENTS system json)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

find_path(MYSQLCPPCONN_INCLUDE_DIR
    NAMES mysql_driver.h
    PATHS /usr/include /usr/local/include
)
find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn
    PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
)

if(NOT MYSQLCPPCONN_INCLUDE_DIR OR NOT MYSQLCPPCONN_LIBRARY)
    message(FATAL_ERROR
        "[server2] MySQL Connector/C++ not found. "
        "Install libmysqlcppconn-dev (headers: mysql_driver.h).")
endif()

message(STATUS "[server2] MYSQLCPPCONN_INCLUDE_DIR=${MYSQLCPPCONN_INCLUDE_DIR}")
message(STATUS "[server2] MYSQLCPPCONN_LIBRARY=${MYSQLCPPCONN_LIBRARY}")

file(GLOB_RECURSE SERVER2_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(SORT SERVER2_SOURCES)

add_executable(CloudMeetingAuthServer ${SERVER2_SOURCES})

target_include_directories(CloudMeetingAuthServer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${MYSQLCPPCONN_INCLUDE_DIR}"
)

target_link_libraries(CloudMeetingAuthServer
    PRIVATE
    Boost::headers
    Boost::system
    Boost::json
    Threads::Threads
    OpenSSL::Crypto
    spdlog::spdlog
    "${MYSQLCPPCONN_LIBRARY}"
)

if(WIN32)
    target_link_libraries(CloudMeetingAuthServer PRIVATE ws2_32 mswsock)
endif()

set_target_properties(CloudMeetingAuthServer PROPERTIES
    OUTPUT_NAME CloudMeetingAuthServer
)

include(GNUInstallDirs)
install(TARGETS CloudMeetingAuthServer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
