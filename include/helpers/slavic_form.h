#pragma once

#include <string>
#include <fmt/format.h>

enum class Case { kNominative, kGenitive, kDative, kAccusative, kInstrumental, kPrepositional };

constexpr auto GetSlavicForm(int number) {
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

constexpr auto SlavicGenerate(const std::array<const char*, 18>& words) {
    return [words](Case c, int n) {
        return fmt::format("{} {}", n, words[3 * static_cast<int>(c) + GetSlavicForm(n)]);
    };
}

constexpr auto GetSlavicDays = SlavicGenerate({
    "дней", "день", "дня",
    "дней", "дня", "дней", 
    "дней", "дню", "дням",
    "дней", "день", "дня",
    "дней", "днём", "днями",
    "дней", "дне", "днях"
});

constexpr auto GetSlavicHours = SlavicGenerate({
    "часов", "час", "часа",
    "часов", "часа", "часов", 
    "часов", "часу", "часам",
    "часов", "час", "часа",
    "часов", "часом", "часами",
    "часов", "часе", "часах"
});

constexpr auto GetSlavicPages = SlavicGenerate({
    "страниц", "страница", "страницы",
    "страниц", "страницы", "страниц",
    "страниц", "странице", "страницам",
    "страниц", "страницу", "страницы",
    "страниц", "страницей", "страницами",
    "страниц", "странице", "страницах"
});

constexpr auto GetSlavicRounds = SlavicGenerate({
    "раундов", "раунд", "раунда",
    "раундов", "раунда", "раундов",
    "раундов", "раунду", "раундам",
    "раундов", "раунд", "раунда",
    "раундов", "раундом", "раундами",
    "раундов", "раунде", "раундах"
});

constexpr auto GetSlavicBooks = SlavicGenerate({
    "книг", "книга", "книги",
    "книг", "книги", "книг",
    "книг", "книге", "книгам",
    "книг", "книгу", "книги",
    "книг", "книгой", "книгами",
    "книг", "книге", "книгах"
});

constexpr auto GetSlavicSymbols = SlavicGenerate({
    "символов", "символ", "символа",
    "символов", "символа", "символов",
    "символов", "символу", "символам",
    "символов", "символ", "символа",
    "символов", "символом", "символами",
    "символов", "символе", "символах"
});