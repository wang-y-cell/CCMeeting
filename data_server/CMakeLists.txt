#-------------------------------------------------
#
# data_server（Boost.Beast HTTP + nlohmann/json + SQLite）
#
#-------------------------------------------------
# Boost::headers / Boost::system 由根 CMake ccmmeeting_ensure_boost_for_servers 提供
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
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

message(STATUS "[data_server] SQLite3_INCLUDE_DIRS=${SQLite3_INCLUDE_DIRS}")
message(STATUS "[data_server] SQLite3_LIBRARIES=${SQLite3_LIBRARIES}")

file(GLOB_RECURSE SERVER2_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(SORT SERVER2_SOURCES)

add_executable(CloudMeetingAuthServer ${SERVER2_SOURCES})

target_include_directories(CloudMeetingAuthServer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(CloudMeetingAuthServer
    PRIVATE
        Boost::headers
        Boost::system
        nlohmann_json::nlohmann_json
        Threads::Threads
        OpenSSL::Crypto
        spdlog::spdlog
        SQLite::SQLite3
)

if(WIN32)
    target_link_libraries(CloudMeetingAuthServer PRIVATE ws2_32 mswsock)
endif()

set_target_properties(CloudMeetingAuthServer PROPERTIES
    OUTPUT_NAME CloudMeetingAuthServer
)

add_custom_command(TARGET CloudMeetingAuthServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:CloudMeetingAuthServer>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config/auth_server.json"
            "$<TARGET_FILE_DIR:CloudMeetingAuthServer>/config/auth_server.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/static"
            "$<TARGET_FILE_DIR:CloudMeetingAuthServer>/static"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:CloudMeetingAuthServer>/uploads"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:CloudMeetingAuthServer>/data"
    COMMENT "Copy auth config, static assets, and data/uploads dirs"
)

include(GNUInstallDirs)
install(TARGETS CloudMeetingAuthServer
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
