#pragma once

#include "nlohmann/json.hpp"
#include "bot.h"

inline void to_json(nlohmann::json& j, InfoContext info) {
    j = {{"last_activity", info.last_activity}, 
         {"days", info.days}};
}

inline void from_json(const nlohmann::json& j, InfoContext& info) {
    info.last_activity = j.at("last_activity");
    info.days = j.at("days");
}

inline void to_json(nlohmann::json& j, std::unordered_map<uint64_t, InfoContext> stats) {
    for (const auto& pair: stats) {
        j[std::to_string(pair.first)] = std::move(pair.second);
    }
}

inline void from_json(const nlohmann::json& j, std::unordered_map<uint64_t, InfoContext>& stats) {
    for (const auto& pair : j.items()) {
        stats[std::stoull(pair.key())] = pair.value();
    }
}

inline void to_json(nlohmann::json& j, FileConfig config) {
    j = {{"offset", config.offset}, 
         {"stats", std::move(config.stats)}};
}

inline void from_json(const nlohmann::json& j, FileConfig& config) {
    config.offset = j.at("config");
    config.stats = j.at("stats");
}



