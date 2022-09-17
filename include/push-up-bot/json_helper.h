#pragma once

#include "nlohmann/json.hpp"
#include "push-up-bot.h"

inline void to_json(nlohmann::json& j, ShockSeries series) {
    j = {{"duration", series.duration},
         {"days", series.days},
         {"start", series.start}};
}

inline void from_json(const nlohmann::json& j, ShockSeries& series) {
    series.duration = j.at("duration");
    series.days = j.at("days");
    series.start = j.at("start");
}

inline void to_json(nlohmann::json& j, User user) {
    j = {{"username", std::move(user.username)}, 
         {"sum_durations", user.sum_durations}};
    if (user.series) {
        j["series"] = std::move(*user.series);
    }
}

inline void from_json(const nlohmann::json& j, User& user) {
    user.username = j.at("username");
    user.sum_durations = j.at("sum_durations");
    if (j.contains("series")) {
        user.series = j.at("series");
    }
}

inline void to_json(nlohmann::json& j, FileConfig config) {
    j["offset"] = config.offset;
    for (const auto& pair: config.users) {
        j["users"][std::to_string(pair.first)] = std::move(pair.second);
    }
}

inline void from_json(const nlohmann::json& j, FileConfig& config) {
    config.offset = j.at("offset");
    auto it = j.find("users");
    if (it == j.end()) {
        return;
    }
    for (const auto& pair : it->items()) {
        config.users[std::stoll(pair.key())] = pair.value();
    }
}