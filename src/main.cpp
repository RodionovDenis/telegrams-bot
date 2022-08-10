#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include "bot.h"
#include "nlohmann/json.hpp"
#include "json_helper.h"
#include "fmt/format.h"

int main() {
    // Bot().SendMessage("Hello, "
    //                                  "World");
    // auto now = std::chrono::system_clock::now();
    // auto current = std::chrono::system_clock::to_time_t(now);
    // auto test = 1659989412ul;
    // std::cout << (test / 3600) / 24 << " " << (current / 3600) / 24 << std::endl;
    // std::cout << ((current / 3600) + 3) % 24 << " " << (current / 60) % (60) << " " << current % 60;
    // FileConfig config;
    // config.stats[1426821001u] = {.last_activity = 1660085855, .days = 5u};
    // config.stats[957596074u] = {.last_activity = 1660057135, .days = 17u};
    // config.offset = 3;

    // nlohmann::json j = config;
    // std::ofstream file{"config.json"};
    // file << std::setw(4) << j << std::flush;

    // FileConfig a = j;
    Bot().Run();
}