#include "reader-bot.h"
#include "nlohmann/json.hpp"
#include "helpers/json_helper.h"
#include "fmt/format.h"
#include "helpers/slavic_form.h"
#include <fstream>
#include <ranges>
#include <codecvt>
#include <locale>

ReaderBot::ReaderBot() : api_(CreateApi(endpoint_, channel_id_)) {
    std::ifstream file(config_name_);
    if (file.is_open()) {
        config_ = nlohmann::json::parse(file);
    }
    update_thread_ = std::thread{&ReaderBot::ThreadUpdateSeries, this};
    reminder_thread_ = std::thread{&ReaderBot::ThreadReminder, this};
}

void ReaderBot::SaveConfig() {
    nlohmann::json j = config_;
    std::ofstream{config_name_} << j;
}

void ReaderBot::Run() {
    while (true) {
        auto responses = api_->GetUpdates(config_.offset, 3600u);
        std::lock_guard guard(mutex_);
        config_.offset = responses.first;
        for (const auto& request: responses.second) {
            if (request.sender_type != SenderType::kPerson) {
                continue;
            }
            HandleRequest(request);
        } 
    }
}

void ReaderBot::UpdateSeries() {
    for (auto& user: config_.users) {
        auto& series = user.second.series; 
        if (series && series->pages >= ShockSeries::kLimitPages) {
            series->rounds++;
            series->pages = 0;
        } else if (series) {
            api_->SendMessage(user.first, fmt::format("Ваш ударный режим ({}) сгорел.", 
                                                       GetSlavicRounds(user.second.series->rounds)));
            series = std::nullopt;
        }
    }
}

auto ReaderBot::GetSortPatricipants() {
    static constexpr auto filter = [](const auto& user) { 
        return user.second.series != std::nullopt; 
    };
    static constexpr auto transform = [](const auto& user) { 
        return std::make_tuple(user.second.series->rounds, user.second.all_pages, user.first, user.second.username);
    };
    auto it = config_.users | std::views::filter(filter) | std::views::transform(transform);
    std::vector<std::tuple<uint16_t, uint32_t, int64_t, std::string>> v = {it.begin(), it.end()};
    std::ranges::sort(v, [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs) > std::tie(rhs);
    });
    return v;
}

void ReaderBot::SendReminder(int64_t id) const {
    auto& series = config_.users.at(id).series;
    api_->SendMessage(id, fmt::format("До завершения текущего раунда чтения осталось 9 часов. "
        "Чтобы продлить ваш ударный режим до {}, вам необходимо прочитать ещё {}. "
        "Поторопитесь!", GetSlavicRounds(series->rounds), GetSlavicPages(series->pages)));
}

void ReaderBot::ThreadReminder() {
    static const auto sleep_for = [this]() -> uint64_t {
        auto time = time_.GetCurrentTime();
        auto day = time_.GetWeekDay();
        int64_t diff;
        if (time.hours < 15) {
            diff = (14 - time.hours) * 3600l + (59 - time.minutes) * 60l + (60 - time.seconds); 
        } else {
            diff = -((time.hours - 15) * 3600l + time.minutes * 60l + time.seconds);
        }
        if ((day == Day::kSunday && time.hours >= 15) || day == Day::kMonday || day == Day::kTuesday || 
            (day == kWednesday && time.hours < 15)) {
            return (Day::kWednesday - day) * 86400u + diff; 
        } else {
            return (Day::kSunday - day) * 86400u + diff;
        }
    };
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(sleep_for()));
        std::lock_guard guard(mutex_);
        for (const auto& user: config_.users) {
            if (user.second.series && user.second.series->pages < ShockSeries::kLimitPages) {
                SendReminder(user.first);
            }
        }
    }
}

void ReaderBot::SendStatictics(auto&& sort_vector, const std::string& interval) {
    if (sort_vector.empty()) {
        api_->SendMessage(channel_id_, "Статистика ударных режимов отсутствует.");
    }
    std::string message = fmt::format("Статистика участников после интервала {}.\n\n", interval);
    auto count = 0;
    for (const auto& [rounds, all_pages, id, username]: sort_vector) {
        message += fmt::format("{}. {} – {} (всего прочитано {}).\n", 
                                ++count, GetReference(id, username), 
                                GetSlavicRounds(rounds), GetSlavicPages(all_pages));
    }
    api_->SendMessage(channel_id_, message);
}

void ReaderBot::ThreadUpdateSeries() {
    static const auto sleep_for = [this]() -> uint64_t {
        auto day = time_.GetWeekDay();
        auto time = time_.GetCurrentTime();
        auto diff = (23 - time.hours) * 3600ul + (59 - time.minutes) * 60ul + (58 - time.seconds); 
        if ((Day::kMonday <= day) && (day <= Day::kWednesday)) {
            return (static_cast<int>(Day::kWednesday) - static_cast<int>(day)) * 86400 + diff;
        } else if (day == Day::kThursday) {
            return diff + 2;
        } else {
            return (static_cast<int>(Day::kSunday) - static_cast<int>(day)) * 86400 + diff;
        }
    };
    static constexpr auto get_interval = [](const Day& day) {
        if (day == Day::kMonday || day == Day::kWednesday) {
            return "понедельник – среда";
        } else {
            return "пятница – воскресенье";
        }
    };
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(sleep_for()));
        std::lock_guard guard(mutex_);
        auto day = time_.GetWeekDay();
        if (day == Day::kFriday) {
            api_->SendMessage(channel_id_, "Начало интервала пятница – воскресенье.");
            break;
        }
        SendStatictics(GetSortPatricipants(), get_interval(day));
        api_->SendMessage(channel_id_, fmt::format("Завершение интервала {}.", get_interval(day)));
        std::this_thread::sleep_for(std::chrono::seconds(3));
        day = time_.GetWeekDay();
        api_->SendMessage(channel_id_, fmt::format("Начало интервала {}.", get_interval(day))); 
        SaveConfig();
    }
}

void ReaderBot::HandleNotConversation(int64_t id, const std::string& message) {
    if (auto it = simple_commands_.find(message); it != simple_commands_.end()) {
        (this->*it->second)(id);
    } else if (hard_commands_.contains(message)) {
        auto conv = CreateConversation(id, channel_id_, api_.get(), &config_.users.at(id), message);
        if (!conv->IsFinish()) {
            current_convers_.emplace(id, std::move(conv));
        }
    } else if (message == "/cancel") {
        api_->SendMessage(id, "Команда не выполняется, мне нечего отменить.");
    } else {
        api_->SendMessage(id, "Вы ввели что-то странное, какую команду мне выполнять? Выберите ее в меню.");
    }
}

void ReaderBot::HandleExistConversation(int64_t id, const std::string& message, Iter it_conv) {
    auto is_command = simple_commands_.contains(message) || hard_commands_.contains(message);
    auto& conv = *it_conv->second;
    if (message == "/cancel") {
        current_convers_.erase(it_conv);
        api_->SendMessage(id, fmt::format("Команда {} отменена.", message));
    } else if (is_command) {
        api_->SendMessage(id, "Очень странно, что ваш ответ – это команда для бота. Если вы "
            "хотите воспользоваться другой командой, сначала вам следует отменить текущую команду "
            "с помощью /cancel.");
    } else if (conv.Handle(message); conv.IsFinish()) {
        current_convers_.erase(it_conv);
    }
}

void ReaderBot::HandleRequest(const RequestBot& request) {
    config_.users.try_emplace(request.id, User{});
    auto id = request.id;
    const auto& message = *request.text;
    auto it = current_convers_.find(id);
    if (it != current_convers_.end()) {
        HandleExistConversation(id, message, it);
    } else {
        HandleNotConversation(id, message);
    }
    // if (it->second->IsFinish()) {
    //     current_convers_.erase(it);
    // }
    SaveConfig();
}

void ReaderBot::SendStart(int64_t id) const {
    std::string text = (config_.users.find(id) == config_.users.end()) ? 
                        "Ты стал участником нашего марафона по чтению." :
                        "Ты уже участвуешь в нашем марафоне по чтению.";
    api_->SendMessage(id, fmt::format("Привет, {}!\n\n{}\n\n"
                                    "Для получения подробной информации нажми /info.", 
                                        config_.users.at(id).username, text));
}

void ReaderBot::SendInfo(int64_t id) const {
    std::ifstream file(read_me_path_);
    if (!file.is_open()) {
        throw std::runtime_error("Read me file is not open");
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    api_->SendMessage(id, stream.str(), ParseMode::kMarkdown);
}

void ReaderBot::SendListBooks(int64_t id) const {
    if (auto it = config_.users.find(id); !it->second.books.empty()) {
        std::string message = "Список ваших зарегистрированных книг: \n\n";
        int count = 0;
        for (const auto& book : it->second.books) {
            message += fmt::format("{}. {} – {} \n", ++count, book.author, book.name);
        }
        api_->SendMessage(id, message);
    } else {
        api_->SendMessage(id, "У вас нет зарегистрированных книг.");
    }
}