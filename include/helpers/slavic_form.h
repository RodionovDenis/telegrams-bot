#pragma once

#include <string>
#include <fmt/format.h>

inline int GetSlavicForm(int number) {
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

inline std::string GetSlavicDays(int n) {
    static constexpr std::array days = {"дней", "день", "дня"};
    return fmt::format("{} {}", n,  days[GetSlavicForm(n)]);
}

inline std::string GetSlavicHours(int n) {
    static constexpr std::array hours = {"часов", "час", "часа"};
    return fmt::format("{} {}", n,  hours[GetSlavicForm(n)]);
}

inline std::string GetSlavicPages(int n) {
    static constexpr std::array pages = {"страниц", "страница", "страницы"};
    return fmt::format("{} {}", n, pages[GetSlavicForm(n)]);
}

inline std::string GetSlavicRounds(int n) {
    static constexpr std::array rounds = {"раундов", "раунд", "раунда"};
    return fmt::format("{} {}", n, rounds[GetSlavicForm(n)]);
}

inline std::string GetSlavicBook(int n) {
    static constexpr std::array books = {"книг", "книга", "книги"};
    return fmt::format("{} {}", n, books[GetSlavicForm(n)]);
}

inline std::string GetSlavicSymbols(int n) {
    static constexpr std::array symbols = {"символов", "символ", "символа"};
    return fmt::format("{} {}", n, symbols[GetSlavicForm(n)]);
}