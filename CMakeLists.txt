cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_ASAN "-g -fsanitize=address,undefined -fno-sanitize-recover=all"
        CACHE STRING "Compiler flags in asan build"
        FORCE)

project(Bots)
add_subdirectory(lib/fmt)

find_package(Poco REQUIRED Data Net)

include_directories(include lib)

add_executable(PushUpBot src/push-up-bot/main.cpp src/push-up-bot/push-up-bot.cpp)
add_executable(ReaderBot src/reader-bot/main.cpp src/reader-bot/reader-bot.cpp)

target_include_directories(PushUpBot PRIVATE include/push-up-bot)

add_library(Conversation src/reader-bot/conversation.cpp)
target_include_directories(Conversation PUBLIC include/reader-bot)

add_library(Api src/api.cpp)

target_link_libraries(Api PRIVATE PocoNetSSL PocoNet PocoFoundation PocoJSON fmt)
target_link_libraries(Conversation PUBLIC fmt)

target_link_libraries(PushUpBot PRIVATE Api pthread fmt)
target_link_libraries(ReaderBot PRIVATE Api Conversation pthread)
