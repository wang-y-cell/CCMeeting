set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)

if(NOT QT_INSTALL_PATH)
    if(DEFINED ENV{Qt6_DIR})
        get_filename_component(QT_INSTALL_PATH "$ENV{Qt6_DIR}/../.." ABSOLUTE)
    else()
        set(QT_INSTALL_PATH "F:/Qt/6.8.3/mingw_64")
    endif()
endif()
list(PREPEND CMAKE_PREFIX_PATH "${QT_INSTALL_PATH}")

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Network)
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)

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
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Threads::Threads
    spdlog::spdlog
)

target_include_directories(CloudMeetingClient PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/configure"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message/video"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/message/text"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/network/messageNet"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/widget"
)

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

include(GNUInstallDirs)
install(TARGETS CloudMeetingClient
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(CloudMeetingClient)
