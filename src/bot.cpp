#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/URI.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include "bot.h"

const std::string Bot::api_ = "https://api.telegram.org/bot5096721296:AAFqVYX1EDPfnI8FAXeU57ESZWTtGjqcYRs/";
const std::string Bot::chat_id_ = "-1001520252639";

void Bot::SendMessage() {
    Poco::URI uri{api_ + "sendMessage"};
    uri.addQueryParameter("chat_id", chat_id_);
    uri.addQueryParameter("text", "[Anastasiia](tg://user?id=1426821001), ты очень красивая девчуля!");
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