#pragma once

#include "nlohmann/json.hpp"
#include "push-up-bot.h"

inline void to_json(nlohmann::json& j, InfoContext info) {
    j = {{"last_activity", info.last_activity},
         {"days", info.days},
         {"start_date", info.start_date}};
}

inline void from_json(const nlohmann::json& j, InfoContext& info) {
    info.last_activity = j.at("last_activity");
    info.days = j.at("days");
    info.start_date = j.at("start_date");
}

inline void to_json(nlohmann::json& j, FileConfig config) {
    j = {{"offset", config.offset}};
    for (const auto& pair: config.stats) {
        j["stats"][std::to_string(pair.first)] = std::move(pair.second);
    }
}

inline void from_json(const nlohmann::json& j, FileConfig& config) {
    config.offset = j.at("offset");
    if (auto it = j.find("stats"); it == j.end()) {
        return;
    }
    for (const auto& pair : j["stats"].items()) {
        config.stats[std::stoull(pair.key())] = pair.value();
    }
}