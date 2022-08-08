#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include "bot.h"
#include "nlohmann/json.hpp"
#include "json_helper.h"
#include "fmt/format.h"

int main() {
    // auto now = std::chrono::system_clock::now();
    // auto current = std::chrono::system_clock::to_time_t(now);
    // std::cout << ((current / 3600) + 3) % 24 << " " << (current / 60) % (60);
    std::unordered_map<uint64_t, InfoContext> map;
    map[1] = {2, 3}; map[2] = {4, 5};

    nlohmann::json j = map;
    std::ofstream file{"jjj.json"};
    file << std::setw(4) << j << std::flush;

    std::unordered_map<uint64_t, InfoContext> c = j;
    std::cout << fmt::format("asad, {}", "hello!");
}