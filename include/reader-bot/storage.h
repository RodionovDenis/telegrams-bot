#pragma once 

#include <string>
#include <unordered_set>
#include <optional>

struct Book {
    static constexpr auto kLimitBooks = 5;
    std::string author;
    std::string name;
    bool operator==(const Book& rhs) const {
        return (author == rhs.author) && (name == rhs.name);
    }
};

class HashBook {
public:
    size_t operator()(const Book& book) const {
        std::hash<std::string> hasher;
        return hasher(book.author) ^ hasher(book.name);
    }
};

struct ShockSeries {
    static constexpr auto kLimitPages = 15u;
    uint32_t rounds = 0;
    uint16_t pages = 0;
};

struct User {
    std::string username;
    uint32_t all_pages = 0;
    std::optional<ShockSeries> series;
    std::unordered_set<Book, HashBook> books;
};
