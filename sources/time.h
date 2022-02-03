#pragma once

#include <format>
#include <string>

#include <cstdint>

constexpr double year_duration = 365.2425 * 24 * 60 * 60;
constexpr double month_duration = year_duration / 12.0;
constexpr uint64_t week_duration = 7 * 24 * 60 * 60;
constexpr uint64_t day_duration = 24 * 60 * 60;
constexpr uint64_t hour_duration = 60 * 60;
constexpr uint64_t minute_duration = 60;
constexpr uint64_t second_duration = 1;

constexpr double years(uint64_t s)
{
    return s / year_duration;
}
constexpr double months(uint64_t s)
{
    return s / month_duration;
}
constexpr double weeks(uint64_t s)
{
    return s / (double)week_duration;
}
constexpr double days(uint64_t s)
{
    return s / (double)day_duration;
}
constexpr double hours(uint64_t s)
{
    return s / (double)hour_duration;
}
constexpr double minutes(uint64_t s)
{
    return s / (double)minute_duration;
}

enum class Time_Unit
{
	seconds,
	minutes,
	hours,
	days,
	weeks,
	months,
	years
};

constexpr uint32_t time_unit_divisors[] = {
    second_duration,
    minute_duration,
    hour_duration,
    day_duration,
    week_duration,
    (uint32_t)month_duration,
    (uint32_t)year_duration
};

static std::string time_unit_labels[] = {
    "seconds",
    "minutes",
    "hours",
    "days",
    "weeks",
    "months",
    "years"
};

static std::string time_unit_short_labels[] = {
    "s",
    "min",
    "hours",
    "days",
    "weeks",
    "months",
    "years"
};

constexpr Time_Unit get_time_unit(uint64_t s)
{
    if (s >= year_duration)
        return Time_Unit::years;
    else if (s >= month_duration)
        return Time_Unit::months;
    else if (s >= week_duration)
        return Time_Unit::weeks;
    else if (s >= day_duration)
        return Time_Unit::days;
    else if (s >= hour_duration)
        return Time_Unit::hours;
    else if (s >= minute_duration)
        return Time_Unit::minutes;
    else
        return Time_Unit::seconds;
}

constexpr uint32_t get_time_unit_divisor(Time_Unit unit)
{
    return time_unit_divisors[(size_t)unit];
}

constexpr std::string get_time_unit_label(Time_Unit unit)
{
    return time_unit_labels[(size_t)unit];
}

constexpr std::string get_time_unit_short_label(Time_Unit unit)
{
    return time_unit_short_labels[(size_t)unit];
}

inline std::string format_duration(uint64_t s)
{
    Time_Unit unit = get_time_unit(s);

    return std::format("{:.1f} {}", s / (double)get_time_unit_divisor(unit), get_time_unit_short_label(unit));
}
