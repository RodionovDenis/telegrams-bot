#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/URI.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/JSON/Parser.h>
#include <ranges>

#include "api.h"
#include "fmt/format.h"

class ApiTelegram : public IApiTelegram {
public:
    ApiTelegram(const std::string& endpoint, int64_t channel_id);
    void SendMessage(int64_t chat_id, const std::string& text, 
                     ParseMode parse_mode,
                     const nlohmann::json& reply_markup, 
                     const std::vector<nlohmann::json>& entities) override;
    std::pair<uint64_t, std::vector<RequestBot>> GetUpdates(uint64_t offset, uint16_t timeout) override;
    int64_t GetAdminID(const std::string& name) override;
    std::unordered_map<int64_t, std::string> GetChatAdmins() override;

private:
    Poco::Dynamic::Var GetReply(const Poco::URI& uri);
    const std::string endpoint_;
    int64_t channel_id_;
};

ApiTelegram::ApiTelegram(const std::string& endpoint, int64_t channel_id) 
: endpoint_(endpoint), channel_id_(channel_id) {
}

Poco::Dynamic::Var ApiTelegram::GetReply(const Poco::URI& uri) {
    Poco::Net::HTTPSClientSession session{uri.getHost(), uri.getPort()};
    Poco::Net::HTTPRequest request{Poco::Net::HTTPRequest::HTTP_GET, uri.getPathAndQuery()};
    session.sendRequest(request);
    Poco::Net::HTTPResponse response;
    auto& body = session.receiveResponse(response);
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        throw std::runtime_error("HTTP_BAD: " + uri.getPathAndQuery());
    }
    Poco::JSON::Parser parser;
    return parser.parse(body);
}

void ApiTelegram::SendMessage(int64_t chat_id, const std::string& text, 
                              ParseMode parse_mode,
                              const nlohmann::json& reply_markup, 
                              const std::vector<nlohmann::json>& entities) {
    Poco::URI uri{endpoint_ + "sendMessage"};
    uri.addQueryParameter("chat_id", std::to_string(chat_id));
    uri.addQueryParameter("text", text);
    if (parse_mode == ParseMode::kMarkdown) {
        uri.addQueryParameter("parse_mode", "Markdown");
    }
    if (!reply_markup.empty()) {
        uri.addQueryParameter("reply_markup", reply_markup.dump());
    }
    if (!entities.empty()) {
        uri.addQueryParameter("entities", nlohmann::json{entities}.front().dump());
    }
    const auto reply = GetReply(uri);
}


int64_t ApiTelegram::GetAdminID(const std::string& name) {
    Poco::URI uri{endpoint_ + "getChatAdministrators"};
    uri.addQueryParameter("chat_id", std::to_string(channel_id_));
    const auto reply = GetReply(uri);
    const auto& result = reply.extract<Poco::JSON::Object::Ptr>()->getArray("result");
    for (const auto& it : *result) {
        const auto& user = it.extract<Poco::JSON::Object::Ptr>()->getObject("user");
        if (user.isNull() || user->getValue<bool>("is_bot")) {
            continue;
        }
        auto username = user->getValue<std::string>("first_name");
        if (user->has("last_name")) {
            username += (" " + user->getValue<std::string>("last_name"));
        }
        if (name == username) {
            return user->getValue<int64_t>("id");
        }
    }
    throw std::runtime_error("Nickname " + name + " is not found!");
}

std::pair<uint64_t, std::vector<RequestBot>> ApiTelegram::GetUpdates(uint64_t offset, uint16_t timeout) {
    Poco::URI uri{endpoint_ + "getUpdates"};
    uri.addQueryParameter("offset", std::to_string(offset));
    uri.addQueryParameter("timeout", std::to_string(timeout));
    const auto reply = GetReply(uri);
    const auto& result = reply.extract<Poco::JSON::Object::Ptr>()->getArray("result");
    std::vector<RequestBot> requests;
    for (const auto& it : *result) {
        const auto& message = it.extract<Poco::JSON::Object::Ptr>()->getObject("message");
        const auto& post = it.extract<Poco::JSON::Object::Ptr>()->getObject("channel_post");
        if ((message.isNull() && post.isNull()) || 
            (!post.isNull() && post->getObject("chat")->getValue<int64_t>("id") != channel_id_) ) {
                continue;
        } else if (!post.isNull() && (post->has("video_note") || post->has("video"))) {
            const auto& username = post->getValue<std::string>("author_signature");
            auto time = post->getValue<uint64_t>("date");
            requests.emplace_back(GetAdminID(username), username, time, SenderType::kChannel);   
        } else if (!message.isNull() && message->has("text")) {
            const auto& chat = message->getObject("chat");
            auto id = chat->getValue<int64_t>("id");
            auto username = chat->getValue<std::string>("first_name");
            if (chat->has("last_name")) {
                username += " " + chat->getValue<std::string>("last_name");   
            }
            auto time = message->getValue<uint64_t>("date");
            requests.emplace_back(id, username, time, SenderType::kPerson, message->getValue<std::string>("text"));
        }
        offset = it.extract<Poco::JSON::Object::Ptr>()->getValue<uint64_t>("update_id") + 1;
    }
    return {offset, requests};
}

std::unordered_map<int64_t, std::string> ApiTelegram::GetChatAdmins() {
    Poco::URI uri{endpoint_ + "getChatAdministrators"};
    uri.addQueryParameter("chat_id", std::to_string(channel_id_));
    const auto reply = GetReply(uri);
    const auto& result = reply.extract<Poco::JSON::Object::Ptr>()->getArray("result");
    std::unordered_map<int64_t, std::string> admins;
    for (const auto& it : *result) {
        const auto& user = it.extract<Poco::JSON::Object::Ptr>()->getObject("user");
        if (user.isNull() || user->getValue<bool>("is_bot")) {
            continue;
        }
        auto username = user->getValue<std::string>("first_name");
        if (user->has("last_name")) {
            username += (" " + user->getValue<std::string>("last_name"));
        }
        auto id = user->getValue<int64_t>("id");
        admins.emplace(id, username);
    }
    return admins;
}

std::unique_ptr<IApiTelegram> CreateApi(const std::string& endpoint, int64_t channel_id) {
    return std::make_unique<ApiTelegram>(endpoint, channel_id);
}

std::string GetReference(int64_t id, const std::optional<std::string>& user) {
    if (user) {
        return fmt::format("[{}](tg://user?id={})", *user, id);
    }
    return fmt::format("tg://user?id={}", id);
}

nlohmann::json AddSpoiler(uint32_t offset, uint32_t length) {
    return  {{"type", "spoiler"}, 
            {"offset", offset}, 
            {"length", length}};
}

nlohmann::json AddTextLink(uint32_t offset, uint32_t length, const std::string& url) {
    return  {{"type", "text_link"}, 
             {"offset", offset}, 
             {"length", length}, 
             {"url", url}};
}