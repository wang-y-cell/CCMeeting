cmake_minimum_required(VERSION 3.16)
project(CloudMeeting VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/client/src/ui"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 可在配置时覆盖: cmake -DQT_INSTALL_PATH="你的Qt路径" ..
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

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/src/*.ui"
)
list(SORT PROJECT_SOURCES)

if(MSVC)
    add_compile_options(/wd"4819")
    add_compile_options(/utf-8)
endif()

qt_add_executable(CloudMeeting
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    client/resource.qrc
)

target_link_libraries(CloudMeeting
    PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Threads::Threads
    spdlog::spdlog
)

target_include_directories(CloudMeeting PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/configure"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/message"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/message/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/message/video"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/message/text"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/network/messageNet"
    "${CMAKE_CURRENT_SOURCE_DIR}/client/include/widget"
)

if(${Qt6_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.yourcompany.CloudMeeting)
endif()
set_target_properties(CloudMeeting PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS CloudMeeting
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(CloudMeeting)
