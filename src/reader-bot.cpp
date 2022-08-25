#include "reader-bot.h"
#include "nlohmann/json.hpp"
#include "helpers/json_helper.h"
#include "fmt/format.h"
#include "helpers/slavic_form.h"
#include <fstream>
#include <ranges>

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

void ReaderBot::Run() {
    while (true) {
        try {
            auto responses = api_->GetUpdates(config_.offset, 3600u);
            config_.offset = responses.first;
            for (const auto& request: responses.second) {
                if (request.sender_type != SenderType::kPerson) {
                    continue;
                }
                HandleParticipant(request);
            }
        } catch (std::exception& ex) {
            std::cout << ex.what() << std::endl;
        } 
    }
}

void ReaderBot::HandleParticipant(const RequestBot& request) {
    std::lock_guard guard(mutex_);
    auto it = currents_id_.find(request.id);
    if (request.text == "/add_session" && it == currents_id_.end()) {
        currents_id_.insert(request.id);
        std::thread{&ReaderBot::AddSession, this, request}.detach();
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
        std::unique_lock lock(mutex_);
        if (auto it = config_.users.find(id); (it != config_.users.end()) && (!it->second.current_books.empty())) {
            api_->SendMessage(id, "Выберите книгу для удаления.", GetButtonsBooks(id));
            auto book = GetPatricipantBook(id, &lock);
            config_.users.at(id).current_books.erase(book);
            api_->SendMessage(id, "Книга успешно удалена.", RemoveButtons());
        } else {
            api_->SendMessage(id, "Нечего удалять.");
        }
        currents_id_.erase(id);
    } catch (const CancelException& ex) {
        api_->SendMessage(id, "Команда отменена.", RemoveButtons());
    }
    SaveConfig();
 }

void ReaderBot::ListBooks(int64_t id) {
    if (auto it = config_.users.find(id); it != config_.users.end() && !it->second.current_books.empty()) {
        std::string message = "Список ваших зарегистрированных книг: \n\n";
        int count = 0;
        for (const auto& book : it->second.current_books) {
            message += fmt::format("{}. {} – {} \n", ++count, book.author, book.name);
        }
        api_->SendMessage(id, message);
    } else {
        api_->SendMessage(id, "У вас нет зарегистрированных книг.");
    }
}

void ReaderBot::AddBook(int64_t id) {
    try {
        std::unique_lock lock(mutex_);
        if (config_.users[id].current_books.size() < kMaxCurrentBooks) {
            api_->SendMessage(id, "Введите автора книги.");
            auto author = WaitRightRequest(id, &lock);
            api_->SendMessage(id, "Введите название книги.");
            auto name = WaitRightRequest(id, &lock);
            config_.users[id].current_books.insert(Book{.author = *author.text, .name = *name.text});
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
        std::unique_lock lock(mutex_);
        auto it = config_.users.find(user_data.id);
        if (it == config_.users.end() || it->second.current_books.empty()) {
            api_->SendMessage(user_data.id, "Сначала необходимо добавить хотя бы одну книгу. Добавьте и повторите попытку.");
            currents_id_.erase(user_data.id);
            return;
        }
        api_->SendMessage(user_data.id, "Выберите книгу, которую читали.", GetButtonsBooks(user_data.id));
        auto book = GetPatricipantBook(user_data.id, &lock);
        api_->SendMessage(user_data.id, "Укажите страницы, которые прочитали (включительно). Страницы должны быть разделены дефисом "
                                        "(например, 25-73). ", RemoveButtons());
        auto pages = GetParticipantPages(user_data.id, &lock);
        api_->SendMessage(user_data.id, fmt::format("Теперь необходимо отчитаться кратким пересказом. Введите текст."));
        auto retell = WaitRightRequest(user_data.id, &lock);
        api_->SendMessage(user_data.id, fmt::format("Поздравляю! Вы прочитали {}. Ваш сеанс добавлен.", 
                                                    GetSlavicPages(pages.second - pages.first + 1)));
        currents_id_.erase(user_data.id);
    } catch (const CancelException& ex) {
        api_->SendMessage(user_data.id, "Команда отменена.", RemoveButtons());
    }
    SaveConfig();
}

std::pair<int, int> ReaderBot::GetParticipantPages(int64_t id, std::unique_lock<std::mutex>* lock) {
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
        auto& books = config_.users.at(id).current_books;
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
    auto v = it->second.current_books | std::views::transform([](const auto& book) {
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