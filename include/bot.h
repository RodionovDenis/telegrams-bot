#include <string>

class Bot {
public:
    void SendMessage();
private:
    static const std::string api_;
    static const std::string chat_id_;
 };