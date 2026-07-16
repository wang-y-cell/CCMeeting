cmake_minimum_required(VERSION 3.16)
project(CloudMeeting VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_CLIENT "Build CloudMeeting Qt client" ON)
option(BUILD_SERVER "Build CloudMeeting server" OFF)

# 至少一个构建选项被选中
if(NOT BUILD_CLIENT AND NOT BUILD_SERVER)
    message(FATAL_ERROR "At least one of BUILD_CLIENT or BUILD_SERVER must be ON.")
endif()

# 设置编译选项
if(MSVC)
    add_compile_options(/wd"4819") #关闭MSVC警告
    add_compile_options(/utf-8) #设置编码
endif()

if(BUILD_CLIENT)
    add_subdirectory(client)
endif()

if(BUILD_SERVER)
    add_subdirectory(server)
endif()
