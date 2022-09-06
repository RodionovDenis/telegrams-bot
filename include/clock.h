#pragma once
#include <iostream>

std::string GetDate();

struct Time {
    uint16_t hours;
    uint16_t minutes;
    uint16_t seconds;
};

class CurrentTime {
public:
    uint16_t DiffDays(uint64_t first_time, uint64_t second_time) const;
    uint16_t DiffIntervals(uint64_t first_time, uint64_t second_time) const;
    Time GetCurrentTime() const;
    uint64_t GetUnixTime() const;

private:
    static constexpr auto kSecondsInDay = 86400u;
    static constexpr auto kSecondsInWeek = 604800u;
    static constexpr auto kUTC = 10800u;
};
