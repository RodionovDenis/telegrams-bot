#pragma once

#include "nlohmann/json.hpp"
#include "push-up-bot.h"
#include "reader-bot.h"

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

inline void to_json(nlohmann::json& j, FileConfigPushUp config) {
    j = {{"offset", config.offset}};
    for (const auto& pair: config.stats) {
        j["stats"][std::to_string(pair.first)] = std::move(pair.second);
    }
}

inline void from_json(const nlohmann::json& j, FileConfigPushUp& config) {
    config.offset = j.at("offset");
    if (auto it = j.find("stats"); it == j.end()) {
        return;
    }
    for (const auto& pair : j["stats"].items()) {
        config.stats[std::stoull(pair.key())] = pair.value();
    }
}

inline void to_json(nlohmann::json& j, Book b) {
    j = {{"author", std::move(b.author)}, 
         {"name", std::move(b.name)}};
}

inline void from_json(const nlohmann::json& j, Book& b) {
    b.author = j.at("author");
    b.name = j.at("name");
}

inline void to_json(nlohmann::json& j, ShockSeries s) {
    j = {{"rounds", s.rounds}, 
         {"pages", s.pages}};
}

inline void from_json(const nlohmann::json& j, ShockSeries& s) {
    s.rounds = j.at("rounds");
    s.pages = j.at("pages");
}

inline void to_json(nlohmann::json& j, User u) {
    j = {{"username", std::move(u.username)},
         {"all_pages", u.all_pages}};
    if (u.series) {
        j["series"] = std::move(*u.series);
    }
    if (!u.books.empty()) {
        j["books"] = std::move(u.books); 
    }
}

inline void from_json(const nlohmann::json& j, User& u) {
    u.username = j.at("username");
    u.all_pages = j.at("all_pages");
    if (auto it = j.find("series"); it != j.end()) {
        u.series = *it;
    }
    if (auto it = j.find("books"); it != j.end()) {
        it->get_to(u.books);
    }
}

inline void to_json(nlohmann::json& j, FileConfigReader config) {
    j = {{"offset", config.offset}};
    for (const auto& pair: config.users) {
        j["users"][std::to_string(pair.first)] = std::move(pair.second);
    }
}

inline void from_json(const nlohmann::json& j, FileConfigReader& config) {
    config.offset = j.at("offset");
    auto it = j.find("users");
    if (it == j.end()) {
        return;
    }
    for (const auto& pair : it->items()) {
        config.users[std::stoll(pair.key())] = pair.value();
    }
}




