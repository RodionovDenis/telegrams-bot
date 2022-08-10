#include <fstream>
#include <thread>

#include "helpers/json_helper.h"
#include "fmt/format.h"
#include "nlohmann/json.hpp"
#include "helpers/slavic_form.h"

#include "bot/bot.h"


//последнее - слелать проверку при обработки видео на тайм
// если больше суток, начать с нуля.

static std::string GetReferenceMessage(const std::string& username, uint64_t id) {
    static const std::string reference = "[{}](tg://user?id={})";
    return fmt::format(reference, username, id);
}

void Bot::ResetOnNewDay() {
    for (auto it = config_.stats.begin(); it != config_.stats.end(); ++it) {
        if (time_.DiffDays(time_.GetUnixTime(), it->second.last_activity) > 1) {
            it = config_.stats.erase(it);
        } else {
            ++it;
        }
    }
    SaveConfig();
}


std::optional<std::string> Bot::GetReminder() {
    std::string message = fmt::format("До сгорания ударных режимов осталось {}.\n\n",
                                        GetSlavicHours(24 - time_.GetCurrentTime().hours));
    bool is_empty = true;
    for (const auto& person : config_.stats) {
        if (!time_.IsSameDay(time_.GetUnixTime(), person.second.last_activity)) {
            auto username = admins_.at(person.first);
            message += fmt::format("{} — {}.\n", GetReferenceMessage(username, person.first),
                                        GetSlavicDays(person.second.days));
            is_empty = false;
        }
    }
    if (is_empty) {
        return std::nullopt;
    }
    message += "\nУспейте отжаться!";
    return message;
}

Bot::Bot() : admins_(api_.SetAdmins()) {
     std::ifstream file_json{config_name_};
     if (!file_json.is_open()) {
         throw std::runtime_error("File " + config_name_ + " is not open!");
     }
     nlohmann::json j = nlohmann::json::parse(file_json);
     j.get_to(config_);
}

void Bot::SaveConfig() {
    nlohmann::json j = config_;
    std::ofstream{config_name_} << j;
}

void Bot::HandleResponse(const Response& responce) {
    if (const auto* p = std::get_if<std::string>(&responce)) {
        HandleText(*p);
    } else if (const auto* v = std::get_if<VideoNote>(&responce)) {
        HandleVideoNote(*v);
    }
    SaveConfig();
}

void Bot::HandleText(const std::string& text) {
    if (text == "/help") {
        api_.SendMessage("Команды для запроса:\n\n"
                        "1. */help* — запрос поддерживаемых команд,\n"
                        "2. */stats_days* — статистика ударного режима всех участников.\n\n"
                        "Планы на будущее (в разработке):\n\n"
                        "1. */stats_push_up* — статистика суммарного количества отжиманий всех участников,\n"
                        "2. */stats_mean* — статистика среднего количества отжиманий в день всех участников.");
    } else if (text == "/stats_days" && !config_.stats.empty()) {
        std::vector<std::pair<int64_t, int64_t>> v;
        for (const auto& stats: config_.stats) {
            v.emplace_back(stats.first, stats.second.days);
        }
        std::sort(v.begin(), v.end(), [](const auto& l1, const auto& l2) {
            return l1.second >= l2.second;
        });
        std::string stats_message = "Статистика ударного режима всех участников:\n\n";
        for (auto i = 0u; i < v.size(); ++i) {
            stats_message += fmt::format("{}. {} — {} ударного режима.\n", i + 1,
                    GetReferenceMessage(admins_[v[i].first], v[i].first) , GetSlavicDays(v[i].second));
        }
        api_.SendMessage(stats_message);
    }
}

void Bot::HandleVideoNote(const VideoNote& video) {
    auto id = GetId(video.username);
    if (auto it = config_.stats.find(id); it != config_.stats.end()) {
        auto diff = time_.DiffDays(video.time, it->second.last_activity);
        if (diff == 1) {
            it->second.days++;
        } else if (diff > 1) {
            it->second.days = 1u;
        }
        it->second.last_activity = video.time;
    } else {
        config_.stats.emplace(id, InfoContext{.last_activity = video.time, .days = 1u});
    }
}

uint64_t Bot::GetId(const std::string& username) {
    for (const auto& pair: admins_) {
        if (pair.second == username) {
            return pair.first;
        }
    }
    auto id = api_.GetId(username);
    admins_[id] = username;
    return id;
}

void Bot::Run() {
    while (true) {
        auto posts = api_.GetUpdates(&config_.offset, 5u);
        for (const auto& post: posts) {
            HandleResponse(post);
        }
        if (time_.IsEven()) {
            api_.SendMessage(GetReminder());
            std::this_thread::sleep_for(std::chrono::seconds(4));
        } else if (time_.IsNewDay()) {
            ResetOnNewDay();
        }
    }
}