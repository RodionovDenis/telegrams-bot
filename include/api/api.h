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

struct Answer {
    uint64_t offset;
    std::vector<Response> data;
};



class TelegramApi {
public:
    void SendMessage(const std::optional<std::string>& text);
    Answer GetUpdates(uint64_t offset, uint16_t timeout);
    std::unordered_map<int64_t, std::string> GetChatAdmins();
    int64_t GetAdminID(const std::string& username);
private:
    const std::string api_ = "https://api.telegram.org/bot5096721296:AAFqVYX1EDPfnI8FAXeU57ESZWTtGjqcYRs/";
    //const int64_t chat_id_ = -1001520252639;
    const int64_t chat_id_ = -1001789330226;
};