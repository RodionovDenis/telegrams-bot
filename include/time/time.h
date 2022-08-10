#pragma once
#include <iostream>

struct Time {
    uint16_t hours;
    uint16_t minutes;
    uint16_t seconds;
};

class CurrentTime {
public:
    bool IsSameDay(uint64_t first_time, uint64_t second_time);
    uint16_t DiffDays(uint64_t first_time, uint64_t second_time);
    Time GetCurrentTime();
    uint64_t GetUnixTime();

private:
    static constexpr auto kSecondsInDay = 86400u;
    static constexpr auto kUTC = 10800u;
};