#pragma once
#include <string>
#include <fmt/format.h>

int GetSlavicForm(int number) {
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

std::string GetSlavicDays(int n) {
    static constexpr std::array days = {"дней", "день", "дня"};
    return fmt::format("{} {}", n,  days[GetSlavicForm(n)]);
}

std::string GetSlavicHours(int n) {
    static constexpr std::array hours = {"часов", "час", "часа"};
    return fmt::format("{} {}", n,  hours[GetSlavicForm(n)]);
}