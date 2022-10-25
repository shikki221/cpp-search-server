#pragma once

#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)

using namespace std::string_literals;

class LogDuration {
public:
    LogDuration() {
    }

    LogDuration(std::string name) : operation_name_(name){
    }

    LogDuration(std::string name, std::ostream& stream)
        : operation_name_(name),
          stream_for_output_(stream){
    }

    ~LogDuration() {
        const auto end_time = std::chrono::steady_clock::now();
        const auto dur = end_time - start_time_;
        stream_for_output_ << operation_name_ << ": "s << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << " ms"s << std::endl;
    }

private:
    const std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
    std::string operation_name_;
    std::ostream& stream_for_output_ = std::cerr;
};