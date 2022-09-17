#include <fstream>
#include <ranges>

#include "json_helper.h"
#include "fmt/format.h"
#include "nlohmann/json.hpp"
#include "helpers/slavic_form.h"

#include "push-up-bot.h"

auto GetTimeDays() {
    auto now = std::chrono::system_clock::now() + std::chrono::hours{3};
    auto days = std::chrono::floor<std::chrono::days>(now);
    auto time = std::chrono::hh_mm_ss{now - days};
    return std::make_pair(time, days - std::chrono::hours{3});
}

std::string GetData(uint64_t unix_time) {
    static constexpr std::array months = {"января", "февраля", "марта", "апреля", "мая",
        "июня", "июля", "августа", "сентября", "октября", "ноября", "декабря"};
    auto now = std::chrono::system_clock::from_time_t(unix_time) + std::chrono::hours{3};
    const std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(now)};
    auto year = static_cast<int>(ymd.year());
    auto month = static_cast<unsigned int>(ymd.month());
    auto day = static_cast<unsigned>(ymd.day());
    return fmt::format("с {} {} {}", day, months[month - 1], year);
}

std::string DurationToTime(uint16_t d) {
    static constexpr std::array funcs = {&GetSlavicDays, &GetSlavicHours, 
        &GetSlavicMinutes, &GetSlavicSeconds};
    std::array durations = {d / 86400, d / 3600 % 24, d % 3600 / 60, d % 60};
    std::string v;
    const auto f = [&v](int n, auto& slavic_form) {
        if (n == 0) {
            return;
        } else if (!v.empty()) {
            v.append(", ");
        }
        v.append(fmt::format("{}", (*slavic_form)(Case::kNominative, n)));
    };
    for (auto i : std::views::iota(0ull, funcs.size())) {
        f(durations[i], funcs[i]);
    }
    return v;
}

void PushUpBot::SendReminderMessage() {
    static constexpr auto kFilter = [](const auto& pair) {
        return pair.second.series && !pair.second.series->duration;
    };
    static constexpr auto kTransform = [](const auto& pair) {
        return std::make_tuple(pair.first, pair.second.username, pair.second.series->days);
    };
    auto views = config_.users | std::views::filter(kFilter) | std::views::transform(kTransform);
    std::vector v(views.begin(), views.end());
    if (v.empty()) {
        return;
    }
    std::ranges::sort(v, std::greater{}, [](const auto& tuple) { return std::get<2>(tuple); });
    auto hour = GetTimeDays().first.hours().count();
    std::string left = ((24 - hour) == 1) ? "остался" : "осталось";
    std::string message = fmt::format("До сгорания ударных режимов {} {}.\n\n",
        left, GetSlavicHours(Case::kNominative, 24 - hour));
    for (const auto& [id, username, days]: v) {
        message += fmt::format("{} — на кону *{}*.\n", GetReference(id, username), 
            GetSlavicDays(Case::kNominative, days));
    }
    message += "\n*Успейте продлить свой ударный режим!*";
    api_->SendMessage(channel_id_, message, ParseMode::kMarkdown);
}

void PushUpBot::RemainderThreadLogic() {
    static constexpr auto kUntil = []() {
        const auto& [time, days] = GetTimeDays();
        auto h = time.hours().count();
        h = std::max(h + 1, 19l) + (h == 23) * 19;
        return days + std::chrono::hours{h};
    };
    while (true) {
        std::this_thread::sleep_until(kUntil());
        std::lock_guard guard(mutex_);
        SendReminderMessage();
    }
}

void PushUpBot::SendLocalDuration() {
    static constexpr auto kFilter = [](const auto& pair) {
        return pair.second.series.has_value() && pair.second.series->duration;
    };
    static constexpr auto kTransform = [](const auto& pair) {
        return std::make_tuple(pair.first, pair.second.username, pair.second.series->duration);
    };
    auto views = config_.users | std::views::filter(kFilter) | std::views::transform(kTransform);
    std::vector v(views.begin(), views.end());
    if (v.empty()) {
        api_->SendMessage(channel_id_, "Сегодня никто не отжимался...");
        return;
    }
    std::ranges::sort(v, std::greater{}, [](const auto& tuple) { return std::get<2>(tuple); });
    auto top = std::make_pair(std::string("Рейтинг участников по продолжительности отжиманий *сегодня*.\n\n"), 0);
    for (const auto& [id, username, d]: v) {
        top.first += fmt::format("{}. {} – {}.\n", ++top.second, GetReference(id, username), DurationToTime(d));
    }
    api_->SendMessage(channel_id_, top.first, ParseMode::kMarkdown);
}

void PushUpBot::SendDays() {
    static constexpr auto kFilter = [](const auto& pair) {
        return pair.second.series.has_value();
    };
    static constexpr auto kTransform = [](auto& pair) {
        auto& series = pair.second.series;
        uint16_t days;
        if (series->duration) {
            pair.second.sum_durations += series->duration;
            series->duration = 0;
            days = ++series->days;
        } else {
            days = series->days;
            series = std::nullopt;
        }
        return std::make_tuple(pair.first, pair.second.username, series.has_value(), days);
    };
    auto views = config_.users | std::views::filter(kFilter) | std::views::transform(kTransform);
    std::vector v(views.begin(), views.end());
    if (v.empty()) {
        return;
    }
    std::ranges::sort(v, std::greater{}, [](const auto& tuple) { return std::get<3>(tuple); });
    auto save = std::make_pair(std::string("Участники, у которых *есть* ударный режим.\n\n"), 0);
    auto lost = std::make_pair(std::string("Участники, *потерявшие* свой ударный режим. \n\n"), 0);
    for (const auto& [id, username, is_series, days]: v) {
        if (is_series) {
            save.first += fmt::format("{}. {} – теперь твой ударный режим {} ({}).\n", ++save.second,
                GetReference(id, username), GetSlavicDays(Case::kNominative, days), 
                GetData(config_.users.at(id).series->start));
        } else {
            lost.first += fmt::format("{}. {} – твой ударный режим ({}) сгорел.\n", ++lost.second,
                GetReference(id, username), GetSlavicDays(Case::kNominative, days));
        }
    }
    if (save.second) {
        api_->SendMessage(channel_id_, save.first, ParseMode::kMarkdown);
    }
    if (lost.second) {
        api_->SendMessage(channel_id_, lost.first, ParseMode::kMarkdown);
    }
}

void PushUpBot::SendGlobalDuration() {
    static constexpr auto kTransform = [](const auto& pair) {
        return std::make_tuple(pair.first, pair.second.username, pair.second.sum_durations);
    };
    auto views = config_.users | std::views::transform(kTransform);
    std::vector v(views.begin(), views.end());
    std::ranges::sort(v, std::greater{}, [](const auto& tuple) { return std::get<2>(tuple); });
    auto top = std::make_pair(std::string("Рейтинг участников по продолжительности отжиманий *за всё время*.\n\n"), 0);
    for (const auto& [id, username, d]: v) {
        top.first += fmt::format("{}. {} – {}.\n", ++top.second, GetReference(id, username), DurationToTime(d));
    }
    api_->SendMessage(channel_id_, top.first, ParseMode::kMarkdown);
}


void PushUpBot::StatsThreadLogic() {
    static constexpr auto kUntil = []() {
        auto days = GetTimeDays().second;
        return days + std::chrono::minutes{15};
    };
    while (true) {
        std::this_thread::sleep_until(kUntil() - std::chrono::seconds{2});
        std::lock_guard guard(mutex_);
        SendLocalDuration();
        SendDays();
        SendGlobalDuration();
        std::this_thread::sleep_for(std::chrono::seconds{2});
        api_->SendMessage(channel_id_, "*Старт нового дня!*", ParseMode::kMarkdown);
        SaveConfig();
    }
    
}

PushUpBot::PushUpBot() : api_(CreateApi(endpoint_, channel_id_)) {
    std::ifstream file_json;
    file_json.open(config_name_);
    if (file_json.is_open()) {
        nlohmann::json j = nlohmann::json::parse(file_json);
        config_ = j;
    }
    remainder_thread_ = std::thread(&PushUpBot::RemainderThreadLogic, this);
    stats_thread_ = std::thread(&PushUpBot::StatsThreadLogic, this);
}

void PushUpBot::SaveConfig() {
    nlohmann::json j = config_;
    std::ofstream{config_name_} << j;
}

void PushUpBot::HandleVideo(const Request& request) {
    auto it = config_.users.find(request.id);
    if (it != config_.users.end()) {
        it->second.username = std::move(request.username);
        auto series = it->second.series.value_or(ShockSeries{.start = request.time});
        series.duration += *request.duration;
        it->second.series = std::move(series);
    } else {
        auto series = ShockSeries{.duration = *request.duration, .start = request.time};
        auto user = User{.username = std::move(request.username), .series = std::move(series)};
        config_.users.emplace(request.id, std::move(user));
    }
    SaveConfig();
}

void PushUpBot::Run() {
    while (true) {
        auto [offset, requests] = api_->GetUpdates(config_.offset, 3600u);
        std::lock_guard guard(mutex_);
        config_.offset = offset;
        for (const auto& request: requests) {
            if (request.request_type != RequestType::kChannel) {
                continue;
            }
            HandleVideo(request);
        }
    }
}