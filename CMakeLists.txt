cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

project(Bots)
add_subdirectory(lib/fmt)

find_package(Poco REQUIRED Data Net)

include_directories(include lib include/push-up-bot include/reader-bot)

add_library(PushUpBot src/push-up-bot/push-up-bot.cpp)
add_library(ReaderBot src/reader-bot/reader-bot.cpp)
add_library(Conversation src/reader-bot/conversation.cpp)
add_library(Api src/api.cpp)
add_library(Clock src/clock.cpp)

add_executable(Bots src/main.cpp)

target_link_libraries(Api PRIVATE PocoNetSSL PocoNet PocoFoundation PocoJSON fmt)
target_link_libraries(Clock fmt)
target_link_libraries(PushUpBot fmt pthread)
target_link_libraries(ReaderBot fmt pthread)
target_link_libraries(Bots PushUpBot ReaderBot Api Clock)
