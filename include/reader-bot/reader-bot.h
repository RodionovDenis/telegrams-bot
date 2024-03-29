#pragma once

#include <mutex>
#include <thread>

#include "api.h"
#include "storage.h"
#include "conversation.h"

struct FileConfig {
    uint64_t offset = 0;
    std::unordered_map<int64_t, User> users;
};

class ReaderBot {
public:
    ReaderBot();
    void Run();

private:
    void HandleRequest(const Request& request);

    std::unordered_map<int64_t, std::unique_ptr<IConversation>> current_convers_;
    using Iter = decltype(current_convers_)::iterator;

    void HandleNotConversation(int64_t id, const std::string& message);
    void HandleExistConversation(int64_t id, const std::string& message, Iter it_conv);

    void SendStart(int64_t id) const;
    void SendInfo(int64_t id) const;
    void SendListBooks(int64_t id) const;
    void SendRules(int64_t id) const;
    const std::unordered_map<std::string, void (ReaderBot::*)(int64_t) const> simple_commands_
        = {{"/start", &ReaderBot::SendStart},
           {"/info", &ReaderBot::SendInfo},
           {"/rules", &ReaderBot::SendRules},
           {"/my_books", &ReaderBot::SendListBooks}};
    std::unordered_set<std::string> hard_commands_ 
        = {"/add_session", "/add_book", "/delete_book"};

    void ThreadUpdateSeries();
    void ThreadReminder();
    static constexpr auto kReminderHour = 15; 
    void SaveConfig();
    void SendReminder(int64_t id) const;

    void SendPages();
    void SendRounds();
    void SendAllPages();

    const std::string endpoint_ = "https://api.telegram.org/bot5659631757:AAEeYIlp3ePkFnxKYXH1yd0bB9AXzgwtOkE/";
    const int64_t channel_id_ = -1001786572579;
    const std::string channel_link_ = "https://t.me/+-vBRibxkUCRmMTUy";
    std::unique_ptr<IApiTelegram> api_;

    FileConfig config_;

    std::mutex mutex_;
    std::thread update_thread_;
    std::thread reminder_thread_;

    const std::string config_name_ = "reader.json";
};