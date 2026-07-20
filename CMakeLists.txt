cmake_minimum_required(VERSION 3.20)
project(iscons)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB EXT_SOURCES ${CMAKE_SOURCE_DIR}/src/ext/*.cpp)
set(SOURCE_FILES src/main.cpp ${EXT_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline)

add_executable(iscons ${SOURCE_FILES})

target_compile_definitions(iscons PRIVATE APP_ROOT="${CMAKE_SOURCE_DIR}")
target_include_directories(iscons PRIVATE ${READLINE_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(iscons PRIVATE ${READLINE_LIBRARIES})
