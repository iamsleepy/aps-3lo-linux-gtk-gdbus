cmake_minimum_required(VERSION 3.26)
project(3lo_aps_shell)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

add_executable(3lo_aps_shell main.cpp)
target_link_libraries(3lo_aps_shell ${GTK3_LIBRARIES})