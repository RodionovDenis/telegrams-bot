#include <fstream>

#include "helpers/json_helper.h"
#include "fmt/format.h"
#include "nlohmann/json.hpp"
#include "helpers/slavic_form.h"

#include "push-up-bot.h"

void PushUpBot::ResetStats() {
    std::erase_if(config_.stats, [this](const auto& pair) {
        return time_.DiffDays(time_.GetUnixTime(), pair.second.last_activity) > 1;
    });
    SaveConfig();
}

void PushUpBot::ShowStats() {
    std::vector<std::pair<int64_t, InfoContext>> v = {config_.stats.begin(), config_.stats.end()};
    std::sort(v.begin(), v.end(), [](const auto& l1, const auto& l2) {
        return l1.second.days >= l2.second.days;
    });
    std::string stats_message = "Статистика ударного режима участников в преддверии " + GetDate() + ":\n\n";
    for (auto i = 0u; i < v.size(); ++i) {
        stats_message += fmt::format("{}. {} — {} ударного режима (с {}).\n", i + 1,
            GetReference(v[i].first, admins_[v[i].first]),
            GetSlavicDays(Case::kInstrumental, v[i].second.days),
            v[i].second.start_date);
    }
    api_->SendMessage(channel_id_, stats_message);
}

std::optional<std::string> PushUpBot::GetReminderMessage() {
    std::string message = fmt::format("До сгорания ударных режимов ровно {}.\n\n",
                                        GetSlavicHours(Case::kInstrumental, 24 - time_.GetCurrentTime().hours));
    bool is_empty = true;
    std::vector<std::pair<int64_t, InfoContext>> v = {config_.stats.begin(), config_.stats.end()};
    std::sort(v.begin(), v.end(), [](const auto& l1, const auto& l2) {
        return l1.second.days >= l2.second.days;
    });
    for (const auto& person: v) {
        if (time_.DiffDays(time_.GetUnixTime(), person.second.last_activity)) {
            auto username = admins_.at(person.first);
            message += fmt::format("{} — {}.\n", GetReference(person.first, username),
                                        GetSlavicDays(Case::kInstrumental, person.second.days));
            is_empty = false;
        }
    }
    if (is_empty) {
        return std::nullopt;
    }
    message += "\nУспейте отжаться во благо здоровью и вашему ударному режиму!";
    return message;
}

void PushUpBot::RemainderThreadLogic() {
    static auto sleep_for = [this]() -> uint64_t {
        auto time = time_.GetCurrentTime();
        auto diff = (60u - time.minutes) * 60u + (60u - time.seconds) - 60u;
        if ((time.hours >= 18) && (time.hours <= 22)) {
            return diff;
        } else if (time.hours < 18) {
            return (18 - time.hours) * 3600u + diff;
        } else {
            return 19u * 3600u +  diff;
        }
    };
    while (true) {
        auto seconds = sleep_for();
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        std::lock_guard guard(mutex_);
        auto reminder_text = GetReminderMessage();
        if (reminder_text) {
            api_->SendMessage(channel_id_, *reminder_text);
        }
    }
}

void PushUpBot::ResetThreadLogic() {
    static auto sleep_for = [this]() -> uint64_t {
        auto time = time_.GetCurrentTime();
        return (23 - time.hours) * 3600u + (60u - time.minutes) * 60u + (60u - time.seconds);
    };
    while (true) {
        auto seconds = sleep_for();
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        std::lock_guard guard(mutex_);
        ResetStats();
        ShowStats();
    }
    
}

PushUpBot::PushUpBot() : api_(CreateApi(endpoint_, channel_id_)), admins_(api_->GetChatAdmins()) {
    std::ifstream file_json;
    file_json.open(config_name_);
    if (!file_json.is_open()) {
        throw std::runtime_error("File " + config_name_ + " is not open!");
    }
    nlohmann::json j = nlohmann::json::parse(file_json);
    config_ = j;
    remainder_thread_ = std::thread(&PushUpBot::RemainderThreadLogic, this);
    reset_thread_ = std::thread(&PushUpBot::ResetThreadLogic, this);
}

PushUpBot::~PushUpBot() {
    remainder_thread_.join();
    reset_thread_.join();
}

void PushUpBot::SaveConfig() {
    nlohmann::json j = config_;
    std::ofstream{config_name_} << j;
}

void PushUpBot::HandleVideo(const RequestBot& request) {
    if (admins_.at(request.id) != request.username) {
        admins_[request.id] = request.username;
    }
    if (auto it = config_.stats.find(request.id); it != config_.stats.end()) {
        auto diff = time_.DiffDays(request.time, it->second.last_activity);
        if (diff == 1) {
            it->second.days++;
        } else if (diff > 1) {
            it->second.days = 1u;
            it->second.start_date = GetDate();
        }
        it->second.last_activity = request.time;
    } else {
        config_.stats.emplace(request.id, InfoContext{.last_activity = request.time, .days = 1u, .start_date = GetDate()});
    }
    SaveConfig();
}

void PushUpBot::Run() {
    while (true) {
        try {
            auto responses = api_->GetUpdates(config_.offset, 3600u);
            std::lock_guard guard(mutex_);
            config_.offset = responses.first;
            for (const auto& request: responses.second) {
                if (request.sender_type != SenderType::kChannel) {
                    continue;
                }
                HandleVideo(request);
            }
        } catch (std::exception& ex) {
            std::cout << ex.what() << std::endl;
        } 
    }
}