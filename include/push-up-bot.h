#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <thread>
#include <mutex>

#include "api.h"
#include "clock.h"

struct InfoContext {
    uint64_t last_activity;
    uint16_t days;
    std::string start_date;
};

struct FileConfigPushUp {
    uint64_t offset = 0;
    std::unordered_map<int64_t, InfoContext> stats;
};

class PushUpBot {
public:
    explicit PushUpBot();
    ~PushUpBot();
    void Run();
private:
    void SaveConfig();
    void HandleVideo(const RequestBot& request);
    std::optional<std::string> GetReminderMessage();
    void ResetStats();
    void ShowStats();
    void RemainderThreadLogic();
    void ResetThreadLogic();

    const std::string endpoint_ = "https://api.telegram.org/bot5524400810:AAGhIs3__dsjSoFHzIHZ932Yorf2xBcK7Aw/";
    const int64_t channel_id_ = -1001610052114;

    std::unique_ptr<IApiTelegram> api_;
    CurrentTime time_;
    
    std::unordered_map<int64_t, std::string> admins_;
    FileConfigPushUp config_;
    const std::string config_name_ = "../config/config.json";

    std::thread remainder_thread_;
    std::thread reset_thread_;
    std::mutex mutex_;
 };