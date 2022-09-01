#pragma once 

#include <memory>

#include "api.h"
#include "storage.h"

class IConversation {
public:
    virtual bool Handle(std::string&& message) = 0;
};

enum class Command {kAddSession, kAddBook, kDeleteBook};
std::unique_ptr<IConversation> CreateConversation(int64_t id, IApiTelegram* api, User* user, Command command);