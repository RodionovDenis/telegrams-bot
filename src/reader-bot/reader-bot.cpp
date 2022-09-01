#include "reader-bot.h"
#include "nlohmann/json.hpp"
#include "helpers/json_helper.h"
#include "fmt/format.h"
#include "helpers/slavic_form.h"
#include <fstream>
#include <ranges>
#include <codecvt>
#include <locale>

static std::pair<std::string, std::string> Parse(const std::string& str, const std::string& del) {
    auto pos = str.find(del);
    if (pos == std::string::npos) {
        return {"", ""};
    }
    std::string first = str.substr(0, pos);
    std::string second = str.substr(pos + del.size(), str.size());
    return {first, second};
}

ReaderBot::ReaderBot() : api_(CreateApi(endpoint_, channel_id_)) {
    std::ifstream file(config_name_);
    if (file.is_open()) {
        config_ = nlohmann::json::parse(file);
    }
    update_thread = std::thread{&ReaderBot::ThreadUpdateSeries, this};
    reminder_thread = std::thread{&ReaderBot::ThreadReminder, this};
}

std::optional<int> GetNumber(const std::string& str) {
    try {
        int number = std::stoi(str);
        return number;
    } catch (...) {
        return std::nullopt;
    }
}

void ReaderBot::SaveConfig() {
    nlohmann::json j = config_;
    std::ofstream{config_name_} << j;
}

std::string ReaderBot::GetReadMe() const {
    std::ifstream file(read_me_path_);
    if (!file.is_open()) {
        throw std::runtime_error("Read me file is not open");
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

void ReaderBot::Run() {
    while (true) {
        auto responses = api_->GetUpdates(config_.offset, 3600u);
        std::lock_guard guard(thread_mutex_);
        config_.offset = responses.first;
        for (const auto& request: responses.second) {
            if (request.sender_type != SenderType::kPerson) {
                continue;
            }
            HandleParticipant(request);
        } 
    }
}

auto ReaderBot::GetSortPatricipants() {
    for (auto& user: config_.users) {
        auto& series = user.second.series; 
        if (series && series->pages >= kPagesLimit) {
            series->rounds++;
            series->pages = 0;
        } else if (series) {
            api_->SendMessage(user.first, fmt::format("Ваш ударный режим ({}) сгорел.", 
                                                       GetSlavicRounds(user.second.series->rounds)));
            series = std::nullopt;
        }
    }
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
        std::lock_guard guard(thread_mutex_);
        for (const auto& user: config_.users) {
            if (user.second.series && user.second.series->rounds > 0 && 
                user.second.series->pages < kPagesLimit) {
                api_->SendMessage(user.first, fmt::format("До завершения текущего раунда чтения осталось 9 часов. "
                                "Чтобы продлить ваш ударный режим до {}, вам необходимо прочитать ещё {}. "
                                "Поторопитесь!", GetSlavicRounds(user.second.series->rounds + 1), 
                                GetSlavicPages(kPagesLimit - user.second.series->pages)));
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
        std::lock_guard guard(thread_mutex_);
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

void ReaderBot::HandleParticipant(const RequestBot& request) {
    if (request.text == "/info") {
        auto str = GetReadMe();
        api_->SendMessage(request.id, str, ParseMode::kMarkdown);
    }
    std::lock_guard guard(handle_request_mutex_);
    auto it = currents_id_.find(request.id);
    if (request.text == "/start" && it == currents_id_.end()) {
        config_.users.insert({request.id, User{.username = request.username}});
        std::string text = (config_.users.find(request.id) == config_.users.end()) ? 
                            "Ты стал участником нашего марафона по чтению." :
                            "Ты уже участвуешь в нашем марафоне по чтению.";
        api_->SendMessage(request.id, fmt::format("Привет, {}!\n\n{}\n\n"
                                      "Для получения подробной информации нажми /info.", request.username, text));
    }
    else if (request.text == "/add_session" && it == currents_id_.end() && time_.GetWeekDay() != Day::kThursday) {
        currents_id_.insert(request.id);
        std::thread{&ReaderBot::AddSession, this, request}.detach();
    } else if (request.text == "/add_session" && it == currents_id_.end() && time_.GetWeekDay() == Day::kThursday) {
        api_->SendMessage(request.id, "Сегодня четверг - выходной день. Запрещено отправлять сеансы чтения. "
                                      "Вы сможете сделать это завтра, когда начнется новый раунд.");
    } else if (request.text == "/add_book" && it == currents_id_.end()) {
        currents_id_.insert(request.id);
        std::thread{&ReaderBot::AddBook, this, request.id}.detach();
    } else if (request.text == "/delete_book" && it == currents_id_.end()) {
        currents_id_.insert(request.id);
        std::thread{&ReaderBot::DeleteBook, this, request.id}.detach();
    } else if (request.text == "/my_books") {
        ListBooks(request.id);
    } else if (request.text == "/cancel" && it == currents_id_.end()) {
        api_->SendMessage(request.id, "Нечего отменять.");
    } else if (request.text == "/cancel" && it != currents_id_.end()) {
        currents_id_.erase(it);
        is_right_task_.notify_all();
    } else if (it != currents_id_.end()) {
        queue_.push(std::move(request));
        is_right_task_.notify_all();
    }
    SaveConfig();
}

class CancelException: public std::exception {
};

void ReaderBot::DeleteBook(int64_t id) {
    try {
        std::unique_lock lock(handle_request_mutex_);
        if (auto it = config_.users.find(id); !it->second.books.empty()) {
            api_->SendMessage(id, "Выберите книгу для удаления.", ParseMode::kNone, GetButtonsBooks(id));
            auto book = GetPatricipantBook(id, &lock);
            it->second.books.erase(book);
            api_->SendMessage(id, "Книга успешно удалена.", ParseMode::kNone, RemoveButtons());
        } else {
            api_->SendMessage(id, "Нечего удалять.");
        }
        currents_id_.erase(id);
    } catch (const CancelException& ex) {
        api_->SendMessage(id, "Команда отменена.", ParseMode::kNone, RemoveButtons());
    }
    SaveConfig();
 }

void ReaderBot::ListBooks(int64_t id) {
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

void ReaderBot::AddBook(int64_t id) {
    try {
        std::unique_lock lock(handle_request_mutex_);
        if (config_.users.at(id).books.size() < kMaxCurrentBooks) { 
            api_->SendMessage(id, "Введите автора книги.");
            auto author = WaitRightRequest(id, &lock);
            api_->SendMessage(id, "Введите название книги.");
            auto name = WaitRightRequest(id, &lock);
            config_.users[id].books.insert(Book{.author = *author.text, .name = *name.text});
            api_->SendMessage(id, "Книга успешно добавлена.");
        } else {
            api_->SendMessage(id, "Превышен лимит на количество книг. Воспользуйтесь удалением.");
        }
        currents_id_.erase(id);
    } catch(const CancelException& ex) {
        api_->SendMessage(id, "Команда отменена.");
    }
    SaveConfig();
}

void ReaderBot::AddSession(const RequestBot& user_data) {
    try {
        std::unique_lock lock(handle_request_mutex_);
        auto it = config_.users.find(user_data.id);
        if (it->second.books.empty()) {
            api_->SendMessage(user_data.id, "Сначала необходимо добавить хотя бы одну книгу. Добавьте и повторите попытку.");
            currents_id_.erase(user_data.id);
            return;
        }
        api_->SendMessage(user_data.id, "Выберите книгу, которую читали.", ParseMode::kNone, GetButtonsBooks(user_data.id));
        auto book = GetPatricipantBook(user_data.id, &lock);
        api_->SendMessage(user_data.id, "Укажите страницы, которые прочитали. Страницы должны быть разделены дефисом "
                                        "(например, 25-73). ", ParseMode::kNone, RemoveButtons());
        auto pages = GetParticipantPages(user_data.id, &lock);
        api_->SendMessage(user_data.id, fmt::format("Теперь необходимо отчитаться кратким пересказом. Введите текст."));
        auto retell = WaitRightRequest(user_data.id, &lock);
        if (!it->second.series) {
            it->second.series = ShockSeries{.rounds = 0, .pages = pages.second - pages.first + 1};
        } else {
            it->second.series->pages += pages.second - pages.first + 1;
        }
        it->second.all_pages += pages.second - pages.first + 1;
        SendRetell(user_data.id, user_data.username, pages.second - pages.first + 1, book, *retell.text);
        api_->SendMessage(user_data.id, fmt::format("Вы прочитали {}. Ваш сеанс добавлен.", 
                                                     GetSlavicPages(pages.second - pages.first + 1)));
        currents_id_.erase(user_data.id);
    } catch (const CancelException& ex) {
        api_->SendMessage(user_data.id, "Команда отменена.", ParseMode::kNone, RemoveButtons());
    }
    SaveConfig();
}

static size_t GetLengthUtf16(const std::string& str) {
    struct Facet : std::codecvt<char16_t, char, std::mbstate_t> {};
    std::wstring_convert<Facet, char16_t> converter;
    return converter.from_bytes(str).size();
}

void ReaderBot::SendRetell(int64_t id, const std::string& username, int pages, const Book& book, 
                           const std::string& retell) {
    auto url = GetReference(id);
    auto text_link = AddTextLink(0, GetLengthUtf16(username), url);
    auto message = fmt::format("{} прочитал(а) {}.\n\nАвтор книги: {}\nНазвание книги: {}\n\nКраткий пересказ: ", 
                                username, GetSlavicPages(pages), book.author, book.name);
    auto spoiler = AddSpoiler(GetLengthUtf16(message), GetLengthUtf16(retell));
    message += retell;
    api_->SendMessage(channel_id_, message, ParseMode::kNone, nlohmann::json{}, {text_link, spoiler});
}

std::string ReaderBot::GetUsername(int64_t id, std::unique_lock<std::mutex>* lock) {
    do {
        auto request = WaitRightRequest(id, lock);
        auto username = Parse(*request.text, " ");
        if (username.first.empty() || username.second.empty()) {
            api_->SendMessage(id, "Некорректное имя пользователя. Введите ещё раз.");
        } else {
            return *request.text;
        }
    } while (true);
}

std::pair<uint32_t, uint32_t> ReaderBot::GetParticipantPages(int64_t id, std::unique_lock<std::mutex>* lock) {
    do {
        auto request = WaitRightRequest(id, lock);
        auto pages = Parse(*request.text, "-");
        auto start = GetNumber(pages.first);
        auto finish = GetNumber(pages.second);
        if (!start || !finish || *start < 0 || *finish < 0 || (*finish - *start < 0)) {
            api_->SendMessage(id, "Некорректный ввод. Повторите попытку.");
        } else {
            return {*start, *finish};
        }
    } while (true);
}

Book ReaderBot::GetPatricipantBook(int64_t id, std::unique_lock<std::mutex>* lock) {
    do {
        auto request = WaitRightRequest(id, lock);
        auto book = Parse(*request.text, " – ");
        auto& books = config_.users.at(id).books;
        if (auto it = books.find(Book{.author = book.first, .name = book.second}); it == books.end()) {
            api_->SendMessage(id, "У вас нет такой книги. Повторите попытку.");
        } else {
            return *it;
        }
    } while (true);
}

RequestBot ReaderBot::WaitRightRequest(int64_t id, std::unique_lock<std::mutex>* lock) {
    while (currents_id_.find(id) != currents_id_.end() && (queue_.empty() || queue_.front().id != id)) {
        is_right_task_.wait(*lock);
    }
    if (currents_id_.find(id) == currents_id_.end()) {
        throw CancelException();
    }
    auto request = std::move(queue_.front());
    queue_.pop();
    return request;
}

nlohmann::json ReaderBot::GetButtonsBooks(int64_t id) const {
    nlohmann::json j;
    auto it = config_.users.find(id);
    if (it == config_.users.end()) {
        throw std::runtime_error("id is not exist, it is strange");
    }
    auto v = it->second.books | std::views::transform([](const auto& book) {
        return book.author + " – " + book.name;
    });
    std::vector<std::vector<std::string>> result;
    for (auto p = v.begin(); p != v.end(); ++p) {
        result.push_back(std::vector<std::string>{*p});
    }
    j["keyboard"] = result;
    j["resize_keyboard"] = true;
    return j;
}

nlohmann::json ReaderBot::RemoveButtons() const {
    nlohmann::json j;
    j["remove_keyboard"] = true;
    return j;
}