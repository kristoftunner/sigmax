#pragma once
#include <fmt/format.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

namespace sigmax {

// TODO: create a logger for each module at some point
#define LOG_DEBUG(...) Logger::GetLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::GetLogger()->info(__VA_ARGS__)
#define LOG_WARN(...) Logger::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::GetLogger()->error(__VA_ARGS__)

class Logger
{
public:
    static void Init();
    static std::shared_ptr<spdlog::logger> GetLogger(){return s_logger;};

    //template<typename... Args> static void Debug(Args &&...args) { s_logger->debug(std::forward<Args>(args)...); }

    //template <typename FormatSrtingType, typename... Args>
    //static void Info(FormatSrtingType&& first, Args&&... rest)
    //{
    //    s_logger->info(std::forward<FormatSrtingType>(first), std::forward<Args>(rest)...);
    //}

    //template<typename... Args> static void Warn(Args &&...args) { s_logger->warn(std::forward<Args>(args)...); }

    //template<typename... Args> static void Error(Args &&...args) { s_logger->error(std::forward<Args>(args)...); }

private:
    static std::shared_ptr<spdlog::logger> s_logger;
    static inline bool s_initialized{ false };
};
}// namespace sigmax
