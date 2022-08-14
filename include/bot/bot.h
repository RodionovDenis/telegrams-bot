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
    std::string start_date;
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
    void HandleResponseNote(const ResponseNote& response);
    void HandleTextNote(const TextNote& note);
    void HandleVideoNote(const VideoNote& note);
    std::optional<std::string> GetReminderMessage();
    void ResetStats();
    uint64_t GetId(const std::string& username);
    void RemainderThreadLogic();
    void ResetThreadLogic();

    TelegramApi api_;
    CurrentTime time_;
    
    std::unordered_map<int64_t, std::string> admins_;
    FileConfig config_;
    const std::string config_name_ = "config.json";

    std::thread remainder_thread_;
    std::thread reset_thread_;
    std::mutex mutex_;
 };