#pragma once

#include <string>
#include <memory>
#include <vector>
#include <variant>
#include <unordered_map>
#include <optional>

enum class SenderType {kChannel, kPerson};

struct RequestBot {
    int64_t id;
    std::string username;
    uint64_t time;
    SenderType sender_type;
    std::optional<std::string> text;
    RequestBot(int64_t i, const std::string& us, uint64_t ti, 
               const SenderType& se, const std::optional<std::string>& te = std::nullopt)
    : id(i), username(us), time(ti), sender_type(se), text(te) {
    }
};

class IApiTelegram {
public:
    virtual void SendMessage(int64_t chat_id, const std::string& text) = 0;
    virtual std::pair<uint64_t, std::vector<RequestBot>> GetUpdates(uint64_t offset, uint16_t timeout) = 0;
    virtual std::unordered_map<int64_t, std::string> GetChatAdmins() = 0;
    virtual int64_t GetAdminID(const std::string& name) = 0;
    virtual ~IApiTelegram() = default;
};


std::unique_ptr<IApiTelegram> CreateApi(const std::string& endpoint, int64_t channel_id);