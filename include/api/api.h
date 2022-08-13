#pragma once

#include <Poco/URI.h>

#include <string>
#include <optional>
#include <vector>
#include <variant>
#include <unordered_map>

struct VideoNote {
    std::string username;
    uint64_t time;
};

struct TextNote {
    std::string text;
    uint64_t time;
};

using ResponseNote = std::variant<TextNote, VideoNote>;

struct Response {
    uint64_t offset;
    std::vector<ResponseNote> data;
};

class TelegramApi {
public:
    void SendMessage(const std::optional<std::string>& text) const;
    Response GetUpdates(uint64_t offset, uint16_t timeout) const;
    std::unordered_map<int64_t, std::string> GetChatAdmins() const;
    int64_t GetAdminID(const std::string& username) const;
private:
    static std::istream& GetBody(const Poco::URI& uri);
    const std::string api_ = "https://api.telegram.org/bot5524400810:AAGhIs3__dsjSoFHzIHZ932Yorf2xBcK7Aw/";
    const int64_t chat_id_ = -1001610052114;
};