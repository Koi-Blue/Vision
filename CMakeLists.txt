cmake_minimum_required(VERSION 3.10)
project(VS)

find_package(OpenCV REQUIRED)

set(EXTRA_LIBS ${OpenCV_LIBS} pthread)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

add_executable(vs 
    src/main.cpp
    src/send.cpp
)

target_link_libraries(vs 
    ${EXTRA_LIBS}
)
