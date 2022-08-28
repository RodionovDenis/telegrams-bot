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
#include "clock.h"

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

struct ShockSeries {
    uint16_t rounds = 0;
    uint64_t last_activity = 0;
    uint32_t pages = 0;
};

struct User {
    std::string username;
    uint32_t all_pages = 0;
    std::optional<ShockSeries> series;
    std::unordered_set<Book, HashBook> books;
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
    void RegistrationParticipant(int64_t id);
    void AddBook(int64_t id);
    void AddSession(const RequestBot& user_data);
    void DeleteBook(int64_t id);
    void ListBooks(int64_t id);
    void ThreadUpdateSeries();
    void ThreadReminder();
private:

    nlohmann::json GetButtonsBooks(int64_t id) const;
    nlohmann::json RemoveButtons() const;
    
    std::string GetUsername(int64_t id, std::unique_lock<std::mutex>* lock);
    std::pair<uint32_t, uint32_t> GetParticipantPages(int64_t id, std::unique_lock<std::mutex>* lock);
    Book GetPatricipantBook(int64_t id, std::unique_lock<std::mutex>* lock); 
    RequestBot WaitRightRequest(int64_t id, std::unique_lock<std::mutex>* lock);
    void SaveConfig();
    std::string GetReadMe() const;
    auto GetSortPatricipants();
    void SendStatictics(auto&& sort_vector, const std::string& interval);
    void SendRetell(int64_t id, const std::string& username, int pages, const Book& book, 
                    const std::string& retell);

    FileConfigReader config_;
    std::unordered_map<int64_t, std::string> participants_;
    std::unordered_set<int64_t> currents_id_;

    static constexpr auto kRoundDays = 3u;
    static constexpr auto kPagesLimit = 15u;
    static constexpr auto kMaxCurrentBooks = 5u;
    CurrentTime time_; 

    const std::string endpoint_ = "https://api.telegram.org/bot5437368583:AAE0XIWHHx3EaDRPTYGyJL0W3R0MeuQiuSc/";
    const int64_t channel_id_ = -1001481144373;

    std::unique_ptr<IApiTelegram> api_;

    std::queue<RequestBot> queue_;
    std::condition_variable is_right_task_;
    std::mutex handle_request_mutex_;
    std::mutex thread_mutex_;
    std::thread update_thread;
    std::thread reminder_thread;

    const std::string config_name_ = "reader.json";
    const std::string read_me_path_ = "../src/reader-bot/read_me.txt";
};