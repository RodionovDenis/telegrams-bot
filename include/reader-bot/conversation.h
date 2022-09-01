#pragma once 

#include <memory>

#include "api.h"
#include "storage.h"

class IConversation {
public:
    virtual ~IConversation() = default;
    virtual void Handle(std::string&& message) = 0;
    bool IsFinish() const {
        return is_finish_;
    }
protected:
    bool is_finish_ = false;
};

enum class Command {kAddSession, kAddBook, kDeleteBook};
std::unique_ptr<IConversation> CreateConversation(int64_t id, IApiTelegram* api, User* user, Command command);