#include "reader-bot.h"
#include "nlohmann/json.hpp"
#include "json_helper.h"
#include "fmt/format.h"
#include "helpers/slavic_form.h"
#include <fstream>
#include <ranges>
#include <codecvt>
#include <locale>

static constexpr const char* info = "*Прежде, чем общаться со мной, прочитай подробное описание моих команд!*\n\n"
    "1. *add_book* – добавлю тебе новую книгу (действует ограничение, ты можешь добавить не более {}).\n"
    "2. *my_books* – покажу текущий список твоих книг.\n"
    "3. *delete_book* – удалю любую книгу из твоего списка.\n"
    "4. *add_session* – зарегистрирую сеанс прочтения (тебе нужно будет выбрать книгу, указать " 
        "страницы и написать пересказ) и опубликую его в общий канал.\n"
    "5. *cancel* – отменю любой запрос, если что-то пошло не так (обращаю внимание, что удаление "
        "сообщения не поможет, нужно воспользоваться этой командой).\n\n"
    "Все эти команды доступны тебе в \"меню\".\n\n"
    "Теперь ты знаком с моими основными командами. Самое время прочитать наши правила – когда и по сколько читать. "
    "Правила доступны тебе по команде /rules.\n\n"
    "Важные ссылки:\n\n"
    "{} с публикациями. Здесь ты сможешь следить за другими участниками, получать статистику, предлагать улучшения, "
        "задавать вопросы.";

static constexpr const char* rules = "*Наши правила: *\n\n"
    "Читаем каждую неделю *двумя раундами*.\n\n"
        "*Раундом* называем следующие временные отрезки:\n\n"
    "1. с *понедельника* 00:00 до *среды* 23:59.\n"
    "2. с *пятницы* 00:00 до *воскресенья* 23:59.\n\n"
    "В каждый раунд тебе нужно прочитать минимум *{}*, но можно и больше. "
    "Соответственно, в неделю нужно читать минимум *{}*. "
    "Первая половина должна быть прочитана в *раунде №1*, вторая половина – в *раунде №2*.\n\n"
    "*Четверг* – выходной день. Все мои команды будут доступны за исключением *add_session*. " 
        "В этот день ей пользоваться нельзя.\n\n"
    "В преддверии завершения очередного раунда в {} я буду отправлять следующую статистику:\n\n"
    "1. топ участников по количеству прочитанных страниц в этом раунде.\n"
    "2. топ учаcтников по ударному режиму (сколько раундов *подряд (!)* ты читаешь больше минимума).\n"
    "3. топ участников по количеству прочитанных страниц за всё время.\n\n"
    "Каждый раунд, за *{}* до его завершения, я буду напоминать тебе о чтении, "
        "если у тебя прочитано менее *{}*.";

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
            GetSlavicPages(Case::kNominative, pages));
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
                GetReference(id, config_.users.at(id).username), GetSlavicRounds(Case::kNominative, rounds));
        } else if (rounds) {
            lost.first += fmt::format("{}. {} – твой ударный режим ({}) сгорел.\n", ++lost.second,
            GetReference(id, config_.users.at(id).username), GetSlavicRounds(Case::kNominative, rounds));
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
            GetSlavicPages(Case::kNominative, pages));
    }
    api_->SendMessage(channel_id_, message, ParseMode::kMarkdown);
}

void ReaderBot::SendReminder(int64_t id) const {
    auto& series = config_.users.at(id).series;
    api_->SendMessage(id, fmt::format("До завершения текущего раунда чтения осталось 9 часов. "
        "Чтобы продлить ваш ударный режим до {}, вам необходимо прочитать ещё {}.\n\n"
        "Поторопитесь!", GetSlavicRounds(Case::kGenitive, series->rounds + 1),
        GetSlavicPages(Case::kAccusative, ShockSeries::kLimitPages - series->pages)));
}

void ReaderBot::ThreadReminder() {
    static constexpr auto kUntil = []() {
        const auto& [time, days, weekday] = GetTimeDaysWeek();
        if ((weekday == std::chrono::Sunday && time.hours().count() >= kReminderHour) || 
            (weekday == std::chrono::Monday) || (weekday == std::chrono::Tuesday) ||
            (weekday == std::chrono::Wednesday && time.hours().count() < kReminderHour)) {
            return days + (std::chrono::Wednesday - weekday) + std::chrono::hours{kReminderHour};
        }
        return days + (std::chrono::Sunday - weekday) + std::chrono::hours{kReminderHour};
    };
    while (true) {
        std::this_thread::sleep_until(kUntil());
        std::lock_guard guard(mutex_);
        for (const auto& user: config_.users) {
            auto& series = user.second.series;
            if (series && series->rounds && series->pages < ShockSeries::kLimitPages) {
                SendReminder(user.first);
            }
        }
    }
}

void ReaderBot::ThreadUpdateSeries() {
    static constexpr auto kUntil = []() {
        const auto& [time, days, weekday] = GetTimeDaysWeek();
        if (weekday == std::chrono::Monday || weekday == std::chrono::Tuesday || weekday == std::chrono::Wednesday) {
            return days + (std::chrono::Wednesday - weekday) + std::chrono::days{1};
        } else if (weekday == std::chrono::Thursday) {
            return days + std::chrono::days{1};
        } else {
            return days + (std::chrono::Sunday - weekday) + std::chrono::days{1};
        }
    };
    static constexpr auto kGetInterval = [](std::chrono::weekday day) {
        if (day == std::chrono::Monday || day == std::chrono::Wednesday) {
            return "понедельник – среда";
        } else {
            return "пятница – воскресенье";
        }
    };
    while (true) {
        std::this_thread::sleep_until(kUntil() - std::chrono::seconds{2});
        std::lock_guard guard(mutex_);
        auto weekday = std::get<2>(GetTimeDaysWeek());
        if (weekday == std::chrono::Thursday) {
            std::this_thread::sleep_for(std::chrono::seconds{2});
            api_->SendMessage(channel_id_, "*Начало раунда пятница – воскресенье.*", ParseMode::kMarkdown);
            break;
        }
        std::erase_if(current_convers_, [this](const auto& pair) {
            api_->SendMessage(pair.first, fmt::format("Команда {} автоматически отменена.", pair.second->What()));
            return true;
        });
        SendPages();
        SendRounds();
        SendAllPages();
        api_->SendMessage(channel_id_, fmt::format("*Завершение раунда {}.*", kGetInterval(weekday)), ParseMode::kMarkdown);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (weekday != std::chrono::Wednesday) {
            api_->SendMessage(channel_id_, fmt::format("*Начало раунда {}.*", kGetInterval(++weekday)), ParseMode::kMarkdown);
        }
        SaveConfig();
    }
}

void ReaderBot::HandleNotConversation(int64_t id, const std::string& message) {
    if (auto it = simple_commands_.find(message); it != simple_commands_.end()) {
        (this->*it->second)(id);
    } else if (hard_commands_.contains(message)) {
        auto weekday = std::get<2>(GetTimeDaysWeek());
        auto conv = CreateConversation(id, channel_id_, api_.get(), &config_.users.at(id), 
            message, weekday);
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
                                    "Для получения подробной информации о поддерживаемых командах "
                                    "нажми /info.", config_.users.at(id).username));
}

void ReaderBot::SendInfo(int64_t id) const {
    api_->SendMessage(id, fmt::format(info, GetSlavicBooks(Case::kGenitive, Book::kLimitBooks),
        GetLink("Общий канал", channel_link_)), ParseMode::kMarkdown);
}

void ReaderBot::SendRules(int64_t id) const {
    api_->SendMessage(id, fmt::format(rules, GetSlavicPages(Case::kDative, ShockSeries::kLimitPages),
        GetSlavicPages(Case::kDative, 2 * ShockSeries::kLimitPages),
        GetLink("общий канал", channel_link_),
        GetSlavicHours(Case::kNominative, 24 - kReminderHour), 
        GetSlavicPages(Case::kGenitive, ShockSeries::kLimitPages)), ParseMode::kMarkdown);
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
