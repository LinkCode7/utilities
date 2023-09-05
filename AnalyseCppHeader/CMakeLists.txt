cmake_minimum_required(VERSION 3.12)
project(AnalyseCppHeader)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# "Qt5Config.cmake"位置
set(Qt5_DIR C:/Qt/Qt5.14.2/5.14.2/msvc2017_64/lib/cmake/Qt5/)

find_package(Qt5 COMPONENTS Widgets Core OpenGL REQUIRED)

# 工程文件
aux_source_directory(. SRCS)

add_executable(${PROJECT_NAME} ${SRCS})

target_include_directories(${PROJECT_NAME} PRIVATE ./iguana)
target_link_libraries(${PROJECT_NAME} Qt5::Widgets Qt5::Core Qt5::OpenGL)

#打印SRC中列表信息
message(STATUS "SRC = ${SRCS}")
