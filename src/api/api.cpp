#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/URI.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/JSON/Parser.h>

#include <api/api.h>

int64_t TelegramApi::GetAdminID(const std::string& name) const {
    Poco::URI uri{api_ + "getChatAdministrators"};
    uri.addQueryParameter("chat_id", std::to_string(chat_id_));
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

std::unordered_map<int64_t, std::string> TelegramApi::GetChatAdmins() const {
    Poco::URI uri{api_ + "getChatAdministrators"};
    uri.addQueryParameter("chat_id", std::to_string(chat_id_));
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

Response TelegramApi::GetUpdates(uint64_t offset, uint16_t timeout) const {
    Poco::URI uri{api_ + "getUpdates"};
    uri.addQueryParameter("offset", std::to_string(offset));
    uri.addQueryParameter("timeout", std::to_string(timeout));
    const auto reply = GetReply(uri);
    const auto& result = reply.extract<Poco::JSON::Object::Ptr>()->getArray("result");
    Response response_result;
    for (const auto& it : *result) {
        const auto& post = it.extract<Poco::JSON::Object::Ptr>()->getObject("channel_post");
        if (post.isNull()) {
                continue;
        }
        const auto& chat = post->getObject("chat");
        if (chat->getValue<int64_t>("id") != chat_id_) {
            continue;
        }
        if (post->has("video_note")) {
            auto username = post->getValue<std::string>("author_signature");
            response_result.data.emplace_back(VideoNote{
                .username = username,
                .time = post->getValue<std::uint64_t>("date")
            });
        } else if (post->has("text")) {
            response_result.data.emplace_back(TextNote{
                .text = post->getValue<std::string>("text"),
                .time = post->getValue<std::uint64_t>("date")
            });
        }
        response_result.offset = it.extract<Poco::JSON::Object::Ptr>()->getValue<uint64_t>("update_id") + 1;
    }
    return response_result;
}

void TelegramApi::SendMessage(const std::optional<std::string>& text) const {
    if (!text) {
        return;
    }
    Poco::URI uri{api_ + "sendMessage"};
    uri.addQueryParameter("chat_id", std::to_string(chat_id_));
    uri.addQueryParameter("text", *text);
    uri.addQueryParameter("parse_mode", "Markdown");
    const auto reply = GetReply(uri);
}

Poco::Dynamic::Var TelegramApi::GetReply(const Poco::URI& uri) {
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