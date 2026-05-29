cmake_minimum_required(VERSION 3.10)
project(CloudMeeting VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_PREFIX_PATH F:/Qt/6.8.3/mingw_64)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Network)

file(GLOB PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/configure/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Img/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/network/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/text/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/video/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Audio/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/logger/*.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/text/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/video/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Img/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/logger/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui"
)
list(SORT PROJECT_SOURCES)

if(MSVC)
    # 忽略 4819 警告（针对某些代码页无法表示的字符）
    add_compile_options(/wd"4819")
    # 强制 MSVC 将源码和执行字符集都设为 UTF-8
    add_compile_options(/utf-8)
endif()

qt_add_executable(CloudMeeting 
    MANUAL_FINALIZATION 
    ${PROJECT_SOURCES}
    resource.qrc
)

target_link_libraries(
    CloudMeeting 
    PRIVATE 
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    # Qt6::Core5Compat
)
target_include_directories(CloudMeeting PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Audio
    ${CMAKE_CURRENT_SOURCE_DIR}/include/video
    ${CMAKE_CURRENT_SOURCE_DIR}/include/configure
    ${CMAKE_CURRENT_SOURCE_DIR}/include/img
    ${CMAKE_CURRENT_SOURCE_DIR}/include/network
    ${CMAKE_CURRENT_SOURCE_DIR}/include/text
    ${CMAKE_CURRENT_SOURCE_DIR}/logger
    ${CMAKE_CURRENT_SOURCE_DIR}
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





