cmake_minimum_required(VERSION 3.16)

project(PushUpBot)
add_subdirectory(lib/fmt)

include_directories(include lib)

add_library(Bot src/bot.cpp)
add_executable(PushUpBot src/main.cpp)
target_link_libraries(Bot PRIVATE PocoNetSSL PocoNet PocoFoundation PocoJSON fmt)
target_link_libraries(PushUpBot Bot fmt)
