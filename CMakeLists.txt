cmake_minimum_required(VERSION 3.23)

project(PushUpBot)
include_directories(include)

add_library(Bot src/bot.cpp)
add_executable(PushUpBot src/main.cpp)
target_link_libraries(PushUpBot Bot PocoNetSSL PocoNet PocoFoundation PocoJSON)