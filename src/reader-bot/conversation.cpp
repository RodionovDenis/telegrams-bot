#include <ranges>

#include "conversation.h"

class AddBook: public IConversation {
public:
    AddBook(int64_t id, IApiTelegram* api, User* user): id_(id), api_(api), user_(user) {
        api_->SendMessage(id_, "Введите автора книги.");
    }

    bool Handle(std::string&& message) override {
        if (author_.empty()) {
            author_ = std::move(message);
            api_->SendMessage(id_, "Введите название книги.");
            return false;
        }
        user_->books.insert({author_, std::move(message)});
        api_->SendMessage(id_, "Книга успешно добавлена.");
        return true;
    }
private:
    int64_t id_;
    IApiTelegram* api_;
    User* user_;
    std::string author_;
};

static std::pair<std::string, std::string> Parse(const std::string& str, const std::string& del) {
    auto pos = str.find(del);
    if (pos == std::string::npos) {
        throw std::invalid_argument("Invalid");
    }
    auto first = str.substr(0, pos);
    auto second = str.substr(pos + del.size(), str.size());
    return {first, second};
}

static nlohmann::json GetButtonsBooks(const std::unordered_set<Book, HashBook>& books) {
    nlohmann::json j;
    auto v = books | std::views::transform([](const auto& book) {
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

static nlohmann::json RemoveButtons() {
    nlohmann::json j;
    j["remove_keyboard"] = true;
    return j;
}

class DeleteBook: public IConversation {
public:
    DeleteBook(int64_t id, IApiTelegram* api, User* user) : id_(id), api_(api), user_(user) {
        api_->SendMessage(id_, "Выберите книгу, которую хотите удалить", GetButtonsBooks(user_->books));
    }

    bool Handle(std::string&& message) override {
        try {
            auto book = Parse(std::move(message), " – ");
            if (user_->books.find(Book{.author = book.first, .name = book.second}) == user_->books.end()) {
                throw std::invalid_argument("Invalid");
            }
            user_->books.erase(Book{.author = book.first, .name = book.second});
            return true;
        } catch(const std::invalid_argument&) {
            api_->SendMessage(id_, "У вас нет такой книги. Выберите книгу из списка или отмените удаление с помощью /cancel.");
            return false;
        }
    }
private:
    int64_t id_;
    IApiTelegram* api_;
    User* user_;
};

class AddSession: public IConversation {
public:
    AddSession(int64_t id, int64_t channel_id, IApiTelegram* api, User* user) 
    : id_(id), channel_id_(channel_id), api_(api), user_(user) {
        api_->SendMessage(id_, "Выберите книгу, которую читали.", GetButtonsBooks(user_->books));
    }

    bool Handle(std::string&& message) override {

    }
    
private:
    int64_t id_;
    int64_t channel_id_;
    IApiTelegram* api_;
    User* user_;
};