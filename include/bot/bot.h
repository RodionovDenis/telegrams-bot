#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <thread>
#include <mutex>

#include "api/api.h"
#include "time/time.h"

struct InfoContext {
    uint64_t last_activity;
    uint16_t days;
};

struct FileConfig {
    uint64_t offset = 0;
    std::unordered_map<int64_t, InfoContext> stats;
};

class Bot {
public:
    explicit Bot();
    ~Bot();
    void Run();
private:
    void SaveConfig();
    void HandleResponse(const Response& response);
    
    void HandleText(const std::string& text);
    void HandleVideoNote(const VideoNote& video);
    std::optional<std::string> GetReminderMessage();
    void ResetNewDay();
    uint64_t GetId(const std::string& username);
    void RemainderThreadLogic();
    void NewDayThreadLogic();

    std::thread remainder_thread_;
    std::thread new_day_thread_;
    std::mutex mutex_;

    TelegramApi api_;
    CurrentTime time_;
    
    std::unordered_map<int64_t, std::string> admins_;
    FileConfig config_;
    const std::string config_name_ = "config.json";
 };