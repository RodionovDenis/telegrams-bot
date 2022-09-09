#include <ranges>
#include <functional>

#include "conversation.h"
#include "fmt/format.h"
#include "helpers/slavic_form.h"

class AddBook: public IConversation {
public:
    AddBook(int64_t id, IApiTelegram* api, User* user) : id_(id), api_(*api), user_(*user) {
        if (user_.books.size() == kLimitBooks) {
            api_.SendMessage(id_, fmt::format("На данный момент действует ограничение – "
                "у вас может быть не больше {}. Воспользуйтесь удалением, затем повторите попытку.", 
                GetSlavicBooks(Case::kGenitive, kLimitBooks)));
            is_finish_ = true;
        } else {
            api_.SendMessage(id_, "Введите автора книги.");
        }
    }

    void Handle(const std::string& message) override {
        if (author_.empty()) {
            author_ = std::move(message);
            api_.SendMessage(id_, "Введите название книги.");
        } else {
            user_.books.insert({author_, message});
            api_.SendMessage(id_, "Книга успешно добавлена.");
            is_finish_ = true;
        }
    }

    std::string What() override {
        return "/add_book";
    }

private:
    std::string author_;
    static constexpr auto kLimitBooks = 5;

    int64_t id_;
    IApiTelegram& api_;
    User& user_;
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

nlohmann::json RemoveButtons() {
    nlohmann::json j;
    j["remove_keyboard"] = true;
    return j;
}

static Book GetBook(const std::string& message, 
                    const std::unordered_set<Book, HashBook>& books) {
    auto book = Parse(message, " – ");
    if (auto it = books.find(Book{.author = book.first, .name = book.second}); it == books.end()) {
        throw std::invalid_argument("Invalid");
    } else {
        return *it;
    }
}

std::pair<int, int> GetNumbers(const std::string& str) {
    auto parse = Parse(std::move(str), "-");
    auto first = std::stoi(parse.first);
    auto second = std::stoi(parse.second);
    if (first <= 0 || second <= 0 || first > second) {
        throw std::invalid_argument("Invalid");
    } 
    return {first, second};
}

size_t GetLengthUtf8(const std::string& message) {
    return std::ranges::count_if(message, [](char c) {
        return (c & 0xc0) != 0x80;
    });
}

size_t GetLengthUtf16(const std::string& message) {
    struct Facet : std::codecvt<char16_t, char, std::mbstate_t> {};
    std::wstring_convert<Facet, char16_t> converter;
    return converter.from_bytes(message).size();
}

class DeleteBook: public IConversation {
public:
    DeleteBook(int64_t id, IApiTelegram* api, User* user) : id_(id), api_(api), user_(user) {
        if (user_->books.empty()) {
            api_->SendMessage(id_, "Нечего удалять, у вас нет книг.");
            is_finish_ = true;
        } else {
            api_->SendMessage(id_, "Выберите из списка книгу, которую хотите удалить.", ParseMode::kNone,
                GetButtonsBooks(user_->books));
        }
    }

    void Handle(const std::string& message) override try {
        auto book = GetBook(message, user_->books);
        user_->books.erase(book);
        api_->SendMessage(id_, "Книга успешно удалена.", ParseMode::kNone, RemoveButtons());
        is_finish_ = true;
    } catch (const std::invalid_argument&) {
            api_->SendMessage(id_, "У вас нет такой книги. Выберите книгу из списка.");
    }

    std::string What() override {
        return "/delete_book";
    }

private:
    int64_t id_;
    IApiTelegram* api_;
    User* user_;
};

class AddSession: public IConversation {
public:
    AddSession(int64_t id, int64_t channel_id, IApiTelegram* api, User* user, std::chrono::weekday day) 
    : id_(id), channel_id_(channel_id), api_(api), user_(user) {
        if (day == std::chrono::Thursday) {
            api_->SendMessage(id_, "Сегодня четверг – выходной день." 
                "Пользоваться командой /add_session запрещено.");
            is_finish_ = true;
        }
        else if (user_->books.empty()) {
            api_->SendMessage(id_, "Чтобы добавить сеанс, у вас должна быть хотя бы одна книга. " 
                "Добавьте книгу и повторите попытку.");
            is_finish_ = true;
        } else {
            api_->SendMessage(id_, "Выберите из списка книгу, которую читали.", ParseMode::kNone, 
            GetButtonsBooks(user_->books));
            state_ = kWaitBook;
            handles_ = {{kWaitBook, &AddSession::HandleBook}, 
                        {kWaitPages, &AddSession::HandlePages}, 
                        {kWaitRetell, &AddSession::HandleRetell}};
        }
    }

    void Handle(const std::string& message) override {
        (this->*handles_[state_])(message);
    }

    std::string What() override {
        return "/add_session";
    }
    
private:
    void HandleBook(const std::string& message) try {
        book_ = GetBook(message, user_->books);
        state_ = kWaitPages;
        api_->SendMessage(id_, "Книга выбрана. Теперь укажите страницы, которые "
            "вы прочитали. Это должны быть два числа, разделенные *дефисом* (например, 54-65).", 
            ParseMode::kMarkdown, RemoveButtons());
    } catch (const std::logic_error&) {
            api_->SendMessage(id_, "У вас нет такой книги. Выберите книгу из списка.");
    }

    void HandlePages(const std::string& message) try {
        auto pages = GetNumbers(message);
        pages_ =  static_cast<uint32_t>(pages.second - pages.first + 1);
        state_ = kWaitRetell;
        api_->SendMessage(id_, fmt::format("Принято. Переходим к пересказу.\n\n"
        "Единственное ограничение на пересказ – не менее *{}* (в среднем, это 2-4 предложения).\n\n"
        "Введите текст.", GetSlavicSymbols(Case::kGenitive, kLimitSymbols)), ParseMode::kMarkdown);
    } catch (const std::logic_error&) {
            api_->SendMessage(id_, "Некорректный ввод страниц.");
    }

    std::string GetAddSessionFinish() {
        auto pages = user_->series->pages; 
        if (pages < ShockSeries::kLimitPages) {
            return fmt::format("\n\nЧтобы остаться в ударном режиме, вам необходимо прочитать еще {} "
                "в текущем раунде.", GetSlavicPages(Case::kGenitive, ShockSeries::kLimitPages - pages));
        } else if (pages > ShockSeries::kLimitPages) {
            return fmt::format("\n\nВ текущем раунде вы прочитали на {} больше минимума.\n\n"
                "Вы остаетесь в ударном режиме.", 
                GetSlavicPages(Case::kAccusative, pages - ShockSeries::kLimitPages));
        }
        return fmt::format("\n\nВ текущем раунде вы прочитали {}.\n\nВы остаетесь в ударном режиме.", 
                GetSlavicPages(Case::kAccusative, ShockSeries::kLimitPages));
    }

    void SendRetellMessage() {
        auto url = GetReference(id_);
        auto text_link = AddTextLink(0, GetLengthUtf16(user_->username), url);
        auto message = fmt::format("{} прочитал(а) {}.\n\nАвтор книги: {}\nНазвание книги: {}\n\nКраткий пересказ: ", 
                                    user_->username, GetSlavicPages(Case::kAccusative, pages_), book_.author, book_.name);
        auto spoiler = AddSpoiler(GetLengthUtf16(message), GetLengthUtf16(retell_));
        message += retell_;
        api_->SendMessage(channel_id_, message, ParseMode::kNone, nlohmann::json{}, {text_link, spoiler});
    }

    void HandleRetell(const std::string& message) {
        auto length = GetLengthUtf8(message);
        if (length < kLimitSymbols) {
            api_->SendMessage(id_, fmt::format("Пересказ не засчитан. У него на {} меньше минимума.\n\n"
            "Повторите попытку.", GetSlavicSymbols(Case::kAccusative, kLimitSymbols - length)));
            return;
        }
        retell_ = std::move(message);
        auto& series = user_->series;
        if (series) { 
            series->pages += pages_;
        } else {
            series = ShockSeries{.rounds = 0, .pages = pages_};
        }
        user_->all_pages += pages_;
        api_->SendMessage(id_, fmt::format("Вы прочитали {}. Ваш сеанс добавлен. {}", GetSlavicPages(Case::kAccusative, pages_), 
            GetAddSessionFinish()));
        SendRetellMessage();
        is_finish_ = true;
    }

    Book book_;
    uint16_t pages_;
    std::string retell_;
    static constexpr auto kLimitSymbols = 250u;

    enum State {kWaitBook, kWaitPages, kWaitRetell};
    State state_;
    std::unordered_map<State, void (AddSession::*)(const std::string&)> handles_;

    int64_t id_;
    int64_t channel_id_;
    IApiTelegram* api_;
    User* user_;
};

std::unique_ptr<IConversation> CreateConversation(int64_t id, int64_t channel_id, IApiTelegram* api, User* user, 
                                                  const std::string& command, std::chrono::weekday day) {
        if (command == "/add_book") {
            return std::make_unique<AddBook>(id, api, user);
        } else if (command == "/delete_book") {
            return std::make_unique<DeleteBook>(id, api, user);
        } else if (command == "/add_session") {
            return std::make_unique<AddSession>(id, channel_id, api, user, day);
        } 
        throw std::runtime_error("Hard command is not found");
    }