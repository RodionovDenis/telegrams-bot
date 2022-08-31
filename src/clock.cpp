#include <chrono>
#include <string>
#include "fmt/format.h"
#include "clock.h"

std::string GetDate() {
     auto now = std::chrono::system_clock::now() + std::chrono::hours(3);
     std::chrono::year_month_day ymd = std::chrono::floor<std::chrono::days>(now);
     static constexpr std::array months = {"января", "февраля", "марта", "апреля", "мая",
        "июня", "июля", "августа", "сентября", "октября", "ноября", "декабря"};
    auto d = static_cast<unsigned>(ymd.day());
    auto m = static_cast<unsigned>(ymd.month());
    auto y = static_cast<int>(ymd.year());
     return fmt::format("{} {} {}", d, months[m - 1], y);
}

Time CurrentTime::GetCurrentTime() const {
    uint64_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    return Time{
        .hours = static_cast<uint16_t>((now / 3600u + 3u) % 24u),
        .minutes = static_cast<uint16_t>((now / 60u) % 60u),
        .seconds = static_cast<uint16_t>(now % 60u)
    };
}

uint64_t CurrentTime::GetUnixTime() const {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

uint16_t CurrentTime::DiffDays(uint64_t first_time, uint64_t second_time) const {
    return ((first_time + kUTC) / kSecondsInDay) - ((second_time + kUTC) / kSecondsInDay);
}

uint16_t CurrentTime::DiffIntervals(uint64_t first_time, uint64_t second_time) const {
    first_time += kUTC; second_time += kUTC;
    first_time -= (first_time / kSecondsInWeek + 1) * kSecondsInDay;
    second_time -= (second_time / kSecondsInWeek + 1) * kSecondsInDay;
    return first_time / (kSecondsInDay * 3) - second_time / (kSecondsInDay * 3);
}

Day CurrentTime::GetWeekDay() const {
    auto now = std::chrono::system_clock::now() + std::chrono::hours(3);
    auto unix_time = std::chrono::system_clock::to_time_t(now);
    return static_cast<Day>(((unix_time / kSecondsInDay) + 3) % 7);
}