#pragma once

#include <unordered_map>
#include <string>
#include <vector>

struct Book {
    std::string author;
    std::string name;
};

struct User {
    std::string username;
    uint16_t rounds;
    uint64_t last_activity;
    uint64_t start_date; 
    std::vector<Book> current_books;
};

struct FileConfigReader {
    uint64_t offset = 0;
    std::unordered_map<int64_t, User> stats;
};

class ReaderBot {
public:
    ReaderBot();
    void HandleText(const std::string& text);
    void AddSession(int64_t id);
    void DeleteBook(const Book& book);
    void ShowBooks(int64_t id);
private:
    FileConfigReader config_;

    static constexpr auto kRoundDays = 3u;
    static constexpr auto kPagesLimit = 15u; 

    const std::string endpoint_ = "5659631757:AAEeYIlp3ePkFnxKYXH1yd0bB9AXzgwtOkE";
    const std::string channel_id_ = "-111111111111111";
    const std::string config_name_ = "../config/reader.json";
};