#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/URI.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/JSON/Parser.h>
#include <fstream>

#include "json_helper.h"
#include "fmt/format.h"
#include "nlohmann/json.hpp"

#include "bot.h"

std::string GetReferenceMessage(const std::string& username, uint64_t id, const std::string& text) {
    static const std::string reference = "[{}](tg://user?id={})";
    return fmt::format(reference, username, id);
}

Bot::Bot() {
    SetAdmins();
    std::ifstream file_json{config_name_};
    if (!file_json.is_open()) {
        throw std::runtime_error("File " + config_name_ + " is not open!");
    }
    nlohmann::json j = nlohmann::json::parse(file_json);
    j.get_to(config_);
}

void Bot::SaveConfig() {
    nlohmann::json j = config_;
    std::ofstream{config_name_} << j;
}

void Bot::Run() {
    while (true) {
        auto posts = GetUpdates(config_.offset, 5u);
        for (const auto& post: posts) {

        }
    }
}

int64_t Bot::GetId(const std::string& name) {
    for (const auto& value: admins_) {
        if (value.second == name) {
            return value.first;
        }
    }
    Poco::URI uri{api_ + "getChatAdministrators"};
    uri.addQueryParameter("chat_id", std::to_string(chat_id_));
    Poco::Net::HTTPSClientSession session{uri.getHost(), uri.getPort()};
    Poco::Net::HTTPRequest request{Poco::Net::HTTPRequest::HTTP_GET, uri.getPathAndQuery()};
    session.sendRequest(request);
    Poco::Net::HTTPResponse response;
    auto& body = session.receiveResponse(response);
    const auto code = response.getStatus();
    if (code != Poco::Net::HTTPResponse::HTTP_OK) {
        throw std::runtime_error("getChatAdministrators is not ok");
    }
    Poco::JSON::Parser parser;
    const auto reply = parser.parse(body);
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
            auto id = user->getValue<int64_t>("id");
            admins_[id] = username;
            return id;
        }
    }
    throw std::runtime_error("Nickname " + name + " is not found!");
}

void Bot::SetAdmins() {
    Poco::URI uri{api_ + "getChatAdministrators"};
    uri.addQueryParameter("chat_id", std::to_string(chat_id_));
    Poco::Net::HTTPSClientSession session{uri.getHost(), uri.getPort()};
    Poco::Net::HTTPRequest request{Poco::Net::HTTPRequest::HTTP_GET, uri.getPathAndQuery()};
    session.sendRequest(request);
    Poco::Net::HTTPResponse response;
    auto& body = session.receiveResponse(response);
    const auto code = response.getStatus();
    if (code != Poco::Net::HTTPResponse::HTTP_OK) {
        throw std::runtime_error("getChatAdministrators is not ok");
    }
    Poco::JSON::Parser parser;
    const auto reply = parser.parse(body);
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
        auto id = user->getValue<int64_t>("id");
        admins_.emplace(id, username);
    }
}

std::vector<Request> Bot::GetUpdates(uint64_t offset, uint16_t timeout) {
    Poco::URI uri{api_ + "getUpdates"};
    uri.addQueryParameter("offset", std::to_string(offset));
    uri.addQueryParameter("timeout", std::to_string(timeout));
    Poco::Net::HTTPSClientSession session{uri.getHost(), uri.getPort()};
    Poco::Net::HTTPRequest request{Poco::Net::HTTPRequest::HTTP_GET, uri.getPathAndQuery()};
    session.sendRequest(request);
    Poco::Net::HTTPResponse response;
    auto& body = session.receiveResponse(response);
    const auto code = response.getStatus();
    if (code != Poco::Net::HTTPResponse::HTTP_OK) {
        throw std::runtime_error("getUpdates is not ok");
    }
    Poco::JSON::Parser parser;
    const auto reply = parser.parse(body);
    const auto& result = reply.extract<Poco::JSON::Object::Ptr>()->getArray("result");
    std::vector<Request> posts;
    for (const auto& it : *result) {
        const auto& post = it.extract<Poco::JSON::Object::Ptr>()->getObject("channel_post");
        if (post.isNull()) {
                continue;
        }
        const auto& chat = it.extract<Poco::JSON::Object::Ptr>()->getObject("chat");
        if (chat->getValue<int64_t>("id") != chat_id_) {
            continue;
        }
        auto username = post->getValue<std::string>("author_signature");
        posts.emplace_back(VideoNote{
            .id = GetId(username),
            .username = username,
            .time = post->getValue<std::uint64_t>("date")
        });
    }
    return posts;
}

void Bot::SendMessage(const std::string& text) {
    Poco::URI uri{api_ + "sendMessage"};
    uri.addQueryParameter("chat_id", std::to_string(chat_id_));
    uri.addQueryParameter("text", text);
    uri.addQueryParameter("parse_mode", "Markdown");
    Poco::Net::HTTPSClientSession session{uri.getHost(), uri.getPort()};
    Poco::Net::HTTPRequest request{Poco::Net::HTTPRequest::HTTP_GET, uri.getPathAndQuery()};
    session.sendRequest(request);
    Poco::Net::HTTPResponse response;
    auto& body = session.receiveResponse(response);
    const auto code = response.getStatus();
    if (code != Poco::Net::HTTPResponse::HTTP_OK) {
        throw std::runtime_error("reaction isn't ok");
    }
}