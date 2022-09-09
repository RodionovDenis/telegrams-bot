#pragma once

#include <mutex>
#include <thread>

#include "api.h"
#include "clock.h"
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
    void HandleRequest(const RequestBot& request);

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
    void SaveConfig();
    void SendReminder(int64_t id) const;

    void SendPages();
    void SendRounds();
    void SendAllPages();

    const std::string endpoint_ = "https://api.telegram.org/bot5437368583:AAE0XIWHHx3EaDRPTYGyJL0W3R0MeuQiuSc/";
    const int64_t channel_id_ = -1001481144373;
    std::unique_ptr<IApiTelegram> api_;

    FileConfig config_;
    CurrentTime time_; 

    std::mutex mutex_;
    std::thread update_thread_;
    std::thread reminder_thread_;

    const std::string config_name_ = "reader.json";
    const std::string info_path_ = "../src/reader-bot/info.txt";
    const std::string rules_path_ = "../src/reader-bot/rules.txt";
};