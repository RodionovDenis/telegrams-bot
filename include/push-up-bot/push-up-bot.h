#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <thread>
#include <mutex>

#include "api.h"

struct ShockSeries {
    bool is_update = false;
    uint16_t days = 0;
    uint64_t start;
};

struct User {
    std::string username;
    uint32_t durations;
    std::optional<ShockSeries> series;
};

struct FileConfig {
    uint64_t offset = 0;
    std::unordered_map<int64_t, User> users;
};

class PushUpBot {
public:
    explicit PushUpBot();
    void Run();
private:
    void SaveConfig();
    void HandleVideo(const Request& request);
    void SendReminderMessage();

    void SendDays();
    void RemainderThreadLogic();
    void StatsThreadLogic();

    const std::string endpoint_ = "https://api.telegram.org/bot5437368583:AAE0XIWHHx3EaDRPTYGyJL0W3R0MeuQiuSc/";
    const int64_t channel_id_ = -1001481144373;
    const std::string config_name_ = "config.json";

    std::unique_ptr<IApiTelegram> api_;
    
    FileConfig config_;

    std::thread remainder_thread_;
    std::thread stats_thread_;
    std::mutex mutex_;
 };