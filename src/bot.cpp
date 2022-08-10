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


//последнее - слелать проверку при обработки видео на тайм
// если больше суток, начать с нуля.

uint64_t CurrentTime() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

std::string GetReferenceMessage(const std::string& username, uint64_t id) {
    static const std::string reference = "[{}](tg://user?id={})";
    return fmt::format(reference, username, id);
}

bool IsSameDays(uint64_t first_unix, uint64_t second_unix) {
    static constexpr auto kSecondsInDay = 86400u;
    static constexpr auto kUTC = 10800u;
    return ((first_unix + kUTC) / kSecondsInDay) == ((second_unix + kUTC) / kSecondsInDay);
}

bool IsResetTime(uint64_t first_unix, uint64_t second_unix) {
    static constexpr auto kMinutesInDay = 1440u;
    return first_unix / 60 - second_unix / 60 > kMinutesInDay;
}

uint16_t GetHours(uint64_t unix_time) {
    return (unix_time / 3600u + 3u) % 24u;
}

uint16_t GetMinutes(uint64_t unix_time) {
    return (unix_time / 60u) % 60u;
}

uint16_t GetSeconds(uint64_t unix_time) {
    return unix_time % 60u;
}

bool CheckEven(uint64_t unix_time) {
    return (GetHours(unix_time) == 3) && (GetMinutes(unix_time) == 35) &&
           (GetSeconds(unix_time) >= 0) && (GetSeconds(unix_time) <= 3);
}

bool CheckNight(uint64_t unix_time) {
    return (GetHours(unix_time) == 0) && (GetMinutes(unix_time) == 0) &&
           (GetSeconds(unix_time) >= 0) && (GetSeconds(unix_time) <= 3);
}



int GetSlavicForm(int number) {
    if ((number % 100 >= 5 && number % 100 <= 20) ||
        (number % 10 >= 5 && number % 10 <= 9) || number % 10 == 0) {
        return 0;
    } else if (number % 10 == 1) {
        return 1;
    } else if (number > 1) {
        return 2;
    } else {
        return 0;
    }
}

std::string GetSlavicDays(int n) {
    static constexpr std::array days = {"дней", "день", "дня"};
    return fmt::format("{} {}", n,  days[GetSlavicForm(n)]);
}

std::string GetSlavicHours(int n) {
    static constexpr std::array hours = {"часов", "час", "часа"};
    return fmt::format("{} {}", n,  hours[GetSlavicForm(n)]);
}

void Bot::Reset() {
    for (auto it = config_.stats.begin(); it != config_.stats.end(); ++it) {
        if (IsResetTime(CurrentTime(), it->second.last_activity)) {
            it = config_.stats.erase(it);
        } else {
            ++it;
        }
    }
    SaveConfig();
}


std::optional<std::string> Bot::GetReminder() {
    auto current_time = CurrentTime();
    std::string message = fmt::format("До сгорания ударных режимов осталось {}.\n\n",
                                        GetSlavicHours(24 - GetHours(current_time)));
    for (const auto& person : config_.stats) {
        if (!IsSameDays(person.second.last_activity, current_time)) {
            auto username = admins_.at(person.first);
            message += fmt::format("{} — {}.\n", GetReferenceMessage(username, person.first),
                                        GetSlavicDays(person.second.days));
        }
    }
    if (message.empty()) {
        return std::nullopt;
    }
    message += "\nУспейте отжаться!";
    return message;
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

void Bot::HandleResponse(const Response& responce) {
    if (const auto* p = std::get_if<std::string>(&responce)) {
        HandleText(*p);
    } else if (const auto* v = std::get_if<VideoNote>(&responce)) {
        HandleVideoNote(*v);
    }
    SaveConfig();
}

void Bot::HandleText(const std::string& text) {
    if (text == "/help") {
        SendMessage("Команды для запроса:\n\n"
                    "1. */help* — запрос поддерживаемых команд,\n"
                    "2. */stats_days* — статистика ударного режима всех участников.\n\n"
                    "Планы на будущее (в разработке):\n\n"
                    "1. */stats_push_up* — статистика суммарного количества отжиманий всех участников,\n"
                    "2. */stats_mean* — статистика среднего количества отжиманий в день всех участников.");
    } else if (text == "/stats_days" && !config_.stats.empty()) {
        std::vector<std::pair<int64_t, int64_t>> v;
        for (const auto& stats: config_.stats) {
            v.emplace_back(stats.first, stats.second.days);
        }
        std::sort(v.begin(), v.end(), [](const auto& l1, const auto& l2) {
            return l1.second >= l2.second;
        });
        std::string stats_message = "Статистика ударного режима всех участников:\n\n";
        for (auto i = 0u; i < v.size(); ++i) {
            stats_message += fmt::format("{}. {} — {} ударного режима.\n", i + 1,
                    GetReferenceMessage(admins_[v[i].first], v[i].first) , GetSlavicDays(v[i].second));
        }
        SendMessage(stats_message);
    }
}

void Bot::HandleVideoNote(const VideoNote& video) {
    if (auto it = config_.stats.find(video.id); it != config_.stats.end()) {
        auto current_time = CurrentTime();
        if (!IsSameDays(current_time, it->second.last_activity)) {
            it->second.days++;
        }
        it->second.last_activity = video.time;
    } else {
        config_.stats.emplace(video.id, InfoContext{.last_activity = video.time, .days = 1u});
    }
}

void Bot::Run() {
    while (true) {
        auto posts = GetUpdates(config_.offset, 5u);
        for (const auto& post: posts) {
            HandleResponse(post);
        }
        auto current_time = CurrentTime();
        if (CheckEven(current_time)) {
            SendMessage(GetReminder());
            std::this_thread::sleep_for(std::chrono::seconds(4));
        } else if (CheckNight(current_time)) {
            Reset();
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

std::vector<Response> Bot::GetUpdates(uint64_t offset, uint16_t timeout) {
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
    std::vector<Response> posts;
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
            posts.emplace_back(VideoNote{
                .id = GetId(username),
                .username = username,
                .time = post->getValue<std::uint64_t>("date")
            });
        } else {
            posts.emplace_back(post->getValue<std::string>("text"));
        }
        config_.offset = it.extract<Poco::JSON::Object::Ptr>()->getValue<uint64_t>("update_id") + 1;
    }
    return posts;
}

void Bot::SendMessage(const std::optional<std::string>& text) {
    if (!text) {
        return;
    }
    Poco::URI uri{api_ + "sendMessage"};
    uri.addQueryParameter("chat_id", std::to_string(chat_id_));
    uri.addQueryParameter("text", *text);
    uri.addQueryParameter("parse_mode", "Markdown");
    Poco::Net::HTTPSClientSession session{uri.getHost(), uri.getPort()};
    Poco::Net::HTTPRequest request{Poco::Net::HTTPRequest::HTTP_GET, uri.getPathAndQuery()};
    session.sendRequest(request);
    Poco::Net::HTTPResponse response;
    auto& body = session.receiveResponse(response);
    const auto code = response.getStatus();
    if (code != Poco::Net::HTTPResponse::HTTP_OK) {
        throw std::runtime_error("SendMessage is not ok");
    }
}