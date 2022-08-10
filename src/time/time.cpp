#include <chrono>
#include <time/time.h>

Time CurrentTime::GetCurrentTime() {
    uint64_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    return Time{
        .hours = static_cast<uint16_t>((now / 3600u + 3u) % 24u),
        .minutes = static_cast<uint16_t>((now / 60u) % 60u),
        .seconds = static_cast<uint16_t>(now % 60u)
    };
}

bool CurrentTime::IsEven() {
    auto time = GetCurrentTime();
    return (time.hours >= 19) && (time.minutes == 00) &&
           (time.seconds >= 0) && (time.seconds <= 3);
}

bool CurrentTime::IsNewDay() {
    auto time = GetCurrentTime();
    return (time.hours == 0) && (time.minutes == 0) &&
           (time.seconds >= 0) && (time.seconds <= 3);
}

uint64_t CurrentTime::GetUnixTime() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

bool CurrentTime::IsSameDay(uint64_t first_time, uint64_t second_time) {
    return ((first_time + kUTC) / kSecondsInDay) == ((second_time + kUTC) / kSecondsInDay);
}

uint16_t CurrentTime::DiffDays(uint64_t first_time, uint64_t second_time) {
    return (first_time / kSecondsInDay) - (second_time / kSecondsInDay);
}