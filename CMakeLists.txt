cmake_minimum_required(VERSION 3.10)
project(jpeg_parser CXX)

set(CMAKE_CXX_STANDARD 17)

# Наше приложение
add_executable(jpeg_parser main.cpp)

# Ищем libjpeg, которую мы заранее собрали и установили в Docker
find_package(JPEG REQUIRED)
target_include_directories(jpeg_parser PRIVATE ${JPEG_INCLUDE_DIR})
target_link_libraries(jpeg_parser PRIVATE ${JPEG_LIBRARIES})