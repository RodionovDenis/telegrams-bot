#include "reader-bot.h"
#include "nlohmann/json.hpp"
#include "helpers/json_helper.h"
#include "fmt/format.h"
#include "helpers/slavic_form.h"
#include <fstream>
#include <ranges>
#include <codecvt>
#include <locale>

auto GetTimeDaysWeek() {
    auto now = std::chrono::system_clock::now() + std::chrono::hours{3};
    auto days = std::chrono::floor<std::chrono::days>(now);
    auto time = std::chrono::hh_mm_ss{now - days};
    auto weekday = std::chrono::weekday{days};
    return std::make_tuple(time, days - std::chrono::hours{3}, weekday);
}

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

void ReaderBot::SendPages() {
    static constexpr auto kFilter = [](const auto& pair) {
        return pair.second.series.has_value() && pair.second.series->pages;
    };
    static constexpr auto kTransform = [](const auto& pair) {
        return std::make_pair(pair.first, pair.second.series->pages);
    };
    auto views = config_.users | std::views::filter(kFilter) | std::views::transform(kTransform);
    std::vector v(views.begin(), views.end());
    std::ranges::sort(v, std::greater{}, &decltype(v)::value_type::second);
    int count = 0;
    std::string message = "Рейтинг участников по количеству прочитанных страниц в текущем раунде.\n\n";
    for (const auto& [id, pages]: v) {
        message += fmt::format("{}. {} – {}.\n", ++count, GetReference(id, config_.users.at(id).username),
            GetSlavicPages(pages));
    }
    if (count) {
        api_->SendMessage(channel_id_, message, ParseMode::kMarkdown);
    } else {
        api_->SendMessage(channel_id_, "В этом раунде никто не читал книги...");
    }
}

void ReaderBot::SendRounds() {
    static constexpr auto kFilter = [](const auto& pair) {
        return pair.second.series.has_value();
    };
    static constexpr auto kTransform = [](auto& pair) {
        auto& series = pair.second.series;
        uint32_t rounds; 
        if (series->pages < ShockSeries::kLimitPages) {
            rounds = series->rounds;
            series = std::nullopt;
        } else {
            rounds = ++series->rounds;
            series->pages = 0;
        }
        return std::make_tuple(pair.first, series.has_value(), rounds);
    };
    auto views = config_.users | std::views::filter(kFilter) | std::views::transform(kTransform);
    std::vector v(views.begin(), views.end());
    std::ranges::sort(v, std::greater{}, [](const auto& tuple) { return std::get<2>(tuple); });
    auto save = std::make_pair(std::string("Участники, у которых *есть* ударный режим.\n\n"), 0);
    auto lost = std::make_pair(std::string("Участники, *потерявшие* свой ударный режим. \n\n"), 0);
    for (const auto& [id, is_series, rounds]: v) {
        if (is_series) {
            save.first += fmt::format("{}. {} – теперь твой ударный режим {}.\n", ++save.second, 
                GetReference(id, config_.users.at(id).username), GetSlavicRounds(rounds));
        } else if (rounds) {
            lost.first += fmt::format("{}. {} – твой ударный режим ({}) сгорел.\n", ++lost.second, 
            GetReference(id, config_.users.at(id).username), GetSlavicRounds(rounds));
        }
    }
    if (save.second) {
        api_->SendMessage(channel_id_, save.first, ParseMode::kMarkdown);
    }
    if (lost.second) {
        api_->SendMessage(channel_id_, lost.first, ParseMode::kMarkdown);
    }
}

void ReaderBot::SendAllPages() {
    static constexpr auto kFilter = [](const auto& pair) -> bool {
        return pair.second.all_pages;
    };
    static constexpr auto kTransform = [](const auto& pair) {
        return std::make_pair(pair.first, pair.second.all_pages);
    };
    auto views = config_.users | std::views::filter(kFilter) | std::views::transform(kTransform);
    std::vector v(views.begin(), views.end());
    std::ranges::sort(v, std::greater{}, &decltype(v)::value_type::second);
    int count = 0;
    std::string message = "Рейтинг участников по количеству прочитанных страниц за всё время.\n\n";
    for (const auto& [id, pages]: v) {
        message += fmt::format("{}. {} – {}.\n", ++count, GetReference(id, config_.users.at(id).username), 
            GetSlavicPages(pages));
    }
    api_->SendMessage(channel_id_, message, ParseMode::kMarkdown);
}

void ReaderBot::SendReminder(int64_t id) const {
    auto& series = config_.users.at(id).series;
    api_->SendMessage(id, fmt::format("До завершения текущего раунда чтения осталось 9 часов. "
        "Чтобы продлить ваш ударный режим до {}, вам необходимо прочитать ещё {}. "
        "Поторопитесь!", GetSlavicRounds(series->rounds), GetSlavicPages(series->pages)));
}

void ReaderBot::ThreadReminder() {
    static constexpr auto kUntil = []() {
        const auto& [time, days, weekday] = GetTimeDaysWeek();
        if ((weekday == std::chrono::Sunday && time.hours().count() >= 15) || 
            (weekday == std::chrono::Wednesday && time.hours().count() < 15) ||
             weekday == std::chrono::Monday || weekday == std::chrono::Tuesday) {

            return days + (std::chrono::Wednesday - weekday) + std::chrono::hours{15};
        }
        return days + (std::chrono::Sunday - weekday) + std::chrono::hours{15};
    };
    while (true) {
        std::this_thread::sleep_until(kUntil());
        std::lock_guard guard(mutex_);
        for (const auto& user: config_.users) {
            if (user.second.series && user.second.series->pages < ShockSeries::kLimitPages) {
                SendReminder(user.first);
            }
        }
    }
}

void ReaderBot::ThreadUpdateSeries() {
    static constexpr auto kUntil = []() {
        // const auto& [time, days, weekday] = GetTimeDaysWeek();
        // if (weekday == std::chrono::Monday || weekday == std::chrono::Tuesday || weekday == std::chrono::Wednesday) {
        //     return days + (std::chrono::Wednesday - weekday) + std::chrono::days{1};
        // } else if (weekday == std::chrono::Thursday) {
        //     return days + std::chrono::days{1};
        // } else {
        //     return days + (std::chrono::Sunday - weekday) + std::chrono::days{1};
        // }
        return std::chrono::minutes{1};
    };
    static constexpr auto kGetInterval = [](std::chrono::weekday day) {
        if (day == std::chrono::Monday || day == std::chrono::Wednesday) {
            return "понедельник – среда";
        } else {
            return "пятница – воскресенье";
        }
    };
    while (true) {
        std::this_thread::sleep_for(kUntil());
        std::lock_guard guard(mutex_);
        auto weekday = std::get<2>(GetTimeDaysWeek());
        if (weekday == std::chrono::Thursday) {
            std::this_thread::sleep_for(std::chrono::seconds{2});
            api_->SendMessage(channel_id_, "*Начало интервала пятница – воскресенье.*", ParseMode::kMarkdown);
            break;
        }
        SendPages();
        SendRounds();
        SendAllPages();
        api_->SendMessage(channel_id_, fmt::format("*Завершение интервала {}.*", kGetInterval(weekday)), ParseMode::kMarkdown);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        api_->SendMessage(channel_id_, fmt::format("*Начало интервала {}.*", kGetInterval(++weekday)), ParseMode::kMarkdown); 
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
        api_->SendMessage(id, "Вы ввели что-то странное, я не знаю такую команду. " 
            "Выберите команду из меню.");
    }
}

void ReaderBot::HandleExistConversation(int64_t id, const std::string& message, Iter it_conv) {
    auto is_command = simple_commands_.contains(message) || hard_commands_.contains(message);
    auto& conv = *it_conv->second;
    if (message == "/cancel") {
        auto command = conv.What();
        current_convers_.erase(it_conv);
        api_->SendMessage(id, fmt::format("Команда {} отменена.", command), 
            ParseMode::kNone, RemoveButtons());
    } else if (is_command) {
        api_->SendMessage(id, "Очень странно, что ваш ответ – это команда для бота. Если вы "
            "хотите воспользоваться другой командой, сначала вам следует отменить текущую команду "
            "с помощью /cancel.");
    } else if (conv.Handle(message); conv.IsFinish()) {
        current_convers_.erase(it_conv);
    }
}

void ReaderBot::HandleRequest(const RequestBot& request) {
    config_.users.try_emplace(request.id, User{.username = request.username});
    auto id = request.id;
    config_.users.at(id).username = std::move(request.username);
    const auto& message = *request.text;
    auto it = current_convers_.find(id);
    if (it != current_convers_.end()) {
        HandleExistConversation(id, message, it);
    } else {
        HandleNotConversation(id, message);
    }
    SaveConfig();
}

void ReaderBot::SendStart(int64_t id) const {
    api_->SendMessage(id, fmt::format("Привет, {}!\n\nТы стал участником нашего марафона по чтению!\n\n"
                                    "Для получения подробной информации о том, как мной пользоваться "
                                    "нажми /info.", config_.users.at(id).username));
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