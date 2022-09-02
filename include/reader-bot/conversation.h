#pragma once 

#include <memory>

#include "api.h"
#include "storage.h"

class IConversation {
public:
    virtual ~IConversation() = default;
    virtual void Handle(const std::string& message) = 0;
    bool IsFinish() const {
        return is_finish_;
    }
protected:
    bool is_finish_ = false;
};

std::unique_ptr<IConversation> CreateConversation(int64_t id, int64_t channel_id, IApiTelegram* api, User* user, 
                                                  const std::string& command);