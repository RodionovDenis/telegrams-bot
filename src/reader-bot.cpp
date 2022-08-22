#include "reader-bot.h"
#include "nlohmann/json.hpp"
#include "helpers/json_helper.h"
#include <fstream>

ReaderBot::ReaderBot() {
    std::ifstream file(config_name_);
    if (file.is_open()) {
        config_ = nlohmann::json::parse(file);
    }
}