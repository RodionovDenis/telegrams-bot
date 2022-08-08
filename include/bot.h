#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct InfoContext {
    uint64_t last_activity;
    uint16_t days;
};

struct FileConfig {
    uint64_t offset = 0;
    std::unordered_map<int64_t, InfoContext> stats;
};

struct Post {
    int64_t id;
    std::string username;
    uint64_t time;
};

class Bot {
public:
    void Run();
private:
    std::vector<Post> GetUpdates(uint64_t offset = 0, uint16_t timeout = 0);
    void SendMessage(const std::string& text);
    void SetAdmins();
    int64_t GetId(const std::string& username);

    std::unordered_map<int64_t, std::string> admins_;
    FileConfig config_;
    const std::string api_ = "https://api.telegram.org/bot5096721296:AAFqVYX1EDPfnI8FAXeU57ESZWTtGjqcYRs/";
    const std::string chat_id_ = "-1001520252639";
 };