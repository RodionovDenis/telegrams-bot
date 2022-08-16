cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

project(PushUpBot)
add_subdirectory(lib/fmt)

find_package(Poco REQUIRED Data Net)

include_directories(include lib)

add_library(Bot src/bot/bot.cpp)
add_library(Api src/api/api.cpp)
add_library(Time src/time/time.cpp)
add_executable(PushUpBot src/main.cpp)
target_link_libraries(Api PRIVATE PocoNetSSL PocoNet PocoFoundation PocoJSON)
target_link_libraries(Time fmt)
target_link_libraries(Bot fmt pthread)
target_link_libraries(PushUpBot Bot Api Time)
