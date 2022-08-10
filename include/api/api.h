#pragma once

#include <string>
#include <optional>
#include <vector>
#include <variant>
#include <unordered_map>

struct VideoNote {
    std::string username;
    uint64_t time;
};

using Response = std::variant<std::string, VideoNote>;

class TelegramApi {
public:
    void SendMessage(const std::optional<std::string>& text);
    std::vector<Response> GetUpdates(uint64_t* offset, uint16_t timeout);
    std::unordered_map<int64_t, std::string> SetAdmins();
    int64_t GetId(const std::string& username);
private:
    const std::string api_ = "https://api.telegram.org/bot5096721296:AAFqVYX1EDPfnI8FAXeU57ESZWTtGjqcYRs/";
    const int64_t chat_id_ = -1001520252639;
};