#include <iostream>
#include <chrono>
#include "bot.h"

int main() {
    auto now = std::chrono::system_clock::now();
    auto current = std::chrono::system_clock::to_time_t(now);
    std::cout << ((current / 3600) + 3) % 24 << " " << (current / 60) % (60);
}