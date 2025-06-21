cmake_minimum_required(VERSION 3.28)
project(linux_chat_client_demo1 C)

set(CMAKE_C_STANDARD 11)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})
add_executable(linux_chat_client_demo1 main.c)

target_link_libraries(linux_chat_client_demo1 ${GTK3_LIBRARIES} pthread)