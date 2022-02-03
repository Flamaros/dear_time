#pragma once

#include <vector>
#include <string_view>
#include <algorithm> 
#include <iterator>

inline std::vector<std::string_view> split_string(const std::string& string, const std::string& delimiter)
{
    std::vector<std::string_view> result;
    size_t start = 0;
    size_t pos = 0;
    std::string_view token;

    result.reserve(256); // @speed
    while ((pos = string.find(delimiter, start)) != std::string::npos) {
        token = std::string_view(&string[start], &string[pos]);
        result.push_back(token);

        start = pos + 1;
    }
    // Add last element
    token = std::string_view(&string[start], &string[string.length()]);
    result.push_back(token);
    return result;
}

inline std::string_view ltrim(std::string_view string)
{
    auto it = std::find_if(string.begin(), string.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        });

    if (it != string.end())
        string.remove_prefix(std::distance(string.begin(), it));
    return string;
}

inline std::string_view rtrim(std::string_view string) {
    auto it = std::find_if(string.rbegin(), string.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        });

    if (it != string.rend())
        string.remove_suffix(std::distance(string.rbegin(), it));
    return string;
}

inline std::string_view trim(std::string_view string) {
    return ltrim(rtrim(string));
}
