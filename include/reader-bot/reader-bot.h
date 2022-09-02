#pragma once

#include <mutex>
#include <thread>

#include "api.h"
#include "clock.h"
#include "storage.h"
#include "conversation.h"

struct FileConfigReader {
    uint64_t offset = 0;
    std::unordered_map<int64_t, User> users;
};

class ReaderBot {
public:
    ReaderBot();
    void Run();

private:
    void HandleRequest(const RequestBot& request);
    

    void SendStart(int64_t id) const;
    void SendInfo(int64_t id) const;
    void SendListBooks(int64_t id) const;
    const std::unordered_map<const char*, void (ReaderBot::*)(int64_t) const> simples_command_
        = {{"/start", &ReaderBot::SendStart},
           {"/info", &ReaderBot::SendInfo},
           {"/my_books", &ReaderBot::SendListBooks}};

    void HandleCancel(int64_t id);

    void ThreadUpdateSeries();
    void ThreadReminder();
    void SaveConfig();
    void SendReminder(int64_t id) const;
    void UpdateSeries();
    auto GetSortPatricipants();
    void SendStatictics(auto&& sort_vector, const std::string& interval);

    std::unique_ptr<IApiTelegram> api_;

    FileConfigReader config_;
    CurrentTime time_;
    std::unordered_map<int64_t, std::unique_ptr<IConversation>> current_convers_; 

    std::mutex mutex_;
    std::thread update_thread_;
    std::thread reminder_thread_;

    const std::string endpoint_ = "https://api.telegram.org/bot5437368583:AAE0XIWHHx3EaDRPTYGyJL0W3R0MeuQiuSc/";
    const int64_t channel_id_ = -1001481144373;
    const std::string config_name_ = "reader.json";
    const std::string read_me_path_ = "../src/reader-bot/read_me.txt";
};