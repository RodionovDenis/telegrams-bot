#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <array>
#include <thread>

struct InfoContext {
    uint64_t last_activity;
    uint16_t days;
};

struct FileConfig {
    uint64_t offset = 0;
    std::unordered_map<int64_t, InfoContext> stats;
};

struct VideoNote {
    int64_t id;
    std::string username;
    uint64_t time;
};

using Response = std::variant<std::string, VideoNote>;

class Bot {
public:
    explicit Bot();
    void Run();
private:
    void SaveConfig();
    void SendMessage(const std::optional<std::string>& text);
    std::vector<Response> GetUpdates(uint64_t offset = 0, uint16_t timeout = 0);
    void SetAdmins();
    int64_t GetId(const std::string& username);
    void HandleResponse(const Response& response);
    void HandleText(const std::string& text);
    void HandleVideoNote(const VideoNote& video);
    std::optional<std::string> GetReminder();
    void Reset();
    
    std::unordered_map<int64_t, std::string> admins_;
    FileConfig config_;
    const std::string api_ = "https://api.telegram.org/bot5096721296:AAFqVYX1EDPfnI8FAXeU57ESZWTtGjqcYRs/";
    const int64_t chat_id_ = -1001520252639;
    const std::string config_name_ = "config.json";
 };