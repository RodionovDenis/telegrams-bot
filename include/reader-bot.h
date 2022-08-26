#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <memory>
#include <queue>
#include <condition_variable>
#include <mutex>
#include "api.h"

struct Book {
    std::string author;
    std::string name;
    bool operator==(const Book& rhs) const {
        return (author == rhs.author) && (name == rhs.name);
    }
};

class HashBook {
public:
    size_t operator()(const Book& book) const {
        std::hash<std::string> hasher;
        return hasher(book.author) ^ hasher(book.name);
    }
};

struct User {
    uint16_t rounds = 0;
    uint64_t last_activity = 0;
    std::string start_date; 
    std::unordered_set<Book, HashBook> current_books;
};

struct FileConfigReader {
    uint64_t offset = 0;
    std::unordered_map<int64_t, User> users;
};

class ReaderBot {
public:
    ReaderBot();
    void Run();
    void HandleParticipant(const RequestBot& request);
    void AddBook(int64_t id);
    void AddSession(const RequestBot& user_data);
    void DeleteBook(int64_t id);
    void ListBooks(int64_t id);
private:

    nlohmann::json GetButtonsBooks(int64_t id) const;
    nlohmann::json RemoveButtons() const;
    
    std::pair<int, int> GetParticipantPages(int64_t id, std::unique_lock<std::mutex>* lock);
    Book GetPatricipantBook(int64_t id, std::unique_lock<std::mutex>* lock); 
    RequestBot WaitRightRequest(int64_t id, std::unique_lock<std::mutex>* lock);
    void SaveConfig();
    std::string GetReadMe() const;

    FileConfigReader config_;
    std::unordered_map<int64_t, std::string> participants_;
    std::unordered_set<int64_t> currents_id_;

    static constexpr auto kRoundDays = 3u;
    static constexpr auto kPagesLimit = 15u;
    static constexpr auto kMaxCurrentBooks = 5u; 

    const std::string endpoint_ = "https://api.telegram.org/bot5659631757:AAEeYIlp3ePkFnxKYXH1yd0bB9AXzgwtOkE/";
    const int64_t channel_id_ = 957596074;

    std::unique_ptr<IApiTelegram> api_;

    std::queue<RequestBot> queue_;
    std::condition_variable is_right_task_;
    std::mutex mutex_;

    const std::string config_name_ = "reader.json";
    const std::string read_me_path_ = "../src/reader-bot/read_me.txt";
};