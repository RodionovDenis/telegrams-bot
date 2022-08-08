cmake_minimum_required(VERSION 3.23)

project(PushUpBot)
add_subdirectory(lib/fmt)

include_directories(include lib lib/fmt/include)

add_library(Bot src/bot.cpp)
add_executable(PushUpBot src/main.cpp)
target_link_libraries(PushUpBot Bot PocoNetSSL PocoNet PocoFoundation PocoJSON fmt)