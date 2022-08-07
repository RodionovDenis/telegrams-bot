#include <string>
#include <vector>
#include <unordered_map>

class Bot {
public:
    std::vector<uint64_t> GetUpdates(uint64_t offset = 0, uint16_t timeout = 0);
    void SendMessage();
    int64_t GetId(const std::string& username);
    void SetAdmins();
private:
    std::unordered_map<int64_t, std::string> admins_;
    std::unordered_map<int64_t, uint64_t> stats_;
    const std::string api_ = "https://api.telegram.org/bot5096721296:AAFqVYX1EDPfnI8FAXeU57ESZWTtGjqcYRs/";
    const std::string chat_id_ = "-1001520252639";
 };