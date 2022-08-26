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

inline void to_json(nlohmann::json& j, std::optional<Statistic> s) {
    if (!s) {
        return;
    }
    j["stat"] = {{"rounds", s->rounds}, 
         {"last_activity", s->last_activity},
         {"pages", s->pages}};
}

inline void from_json(const nlohmann::json& j, std::optional<Statistic>& s) {
    auto it = j.find("stat");
    if (it != j.end()) {
        s = Statistic{
            .rounds = it->at("rounds"),
            .last_activity = it->at("last_activity"),
            .pages = it->at("pages")
        };
    }
}

inline void to_json(nlohmann::json& j, User u) {
    j = {{"username", std::move(u.username)}, 
         {"stat", std::move(u.stat)}, 
         {"books", std::move(u.books)}};
}

inline void from_json(const nlohmann::json& j, User& u) {
    u.username = j.at("username");
    u.stat = j.at("stat");
    j.at("books").get_to(u.books);
}

inline void to_json(nlohmann::json& j, FileConfigReader config) {
    j = {{"offset", config.offset}};
    for (const auto& pair: config.users) {
        j["users"][std::to_string(pair.first)] = std::move(pair.second);
    }
}

inline void from_json(const nlohmann::json& j, FileConfigReader& config) {
    config.offset = j.at("offset");
    if (auto it = j.find("users"); it == j.end()) {
        return;
    }
    for (const auto& pair : j["users"].items()) {
        config.users[std::stoull(pair.key())] = pair.value();
    }
}




