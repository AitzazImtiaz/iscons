cmake_minimum_required(VERSION 3.20)
project(iscons)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES src/main.cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline) # add GNU readline

add_executable(iscons ${SOURCE_FILES})
target_include_directories(iscons PRIVATE ${READLINE_INCLUDE_DIRS})
target_link_libraries(iscons PRIVATE ${READLINE_LIBRARIES})
