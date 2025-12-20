#pragma once
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

namespace sigmax {
class Logger
{
public:
    static void Init();

    template<typename... Args> static void Debug(Args &&...args) { s_logger->debug(std::forward<Args>(args)...); }

    template<typename... Args> static void Info(Args &&...args) { s_logger->info(std::forward<Args>(args)...); }

    template<typename... Args> static void Warn(Args &&...args) { s_logger->warn(std::forward<Args>(args)...); }

    template<typename... Args> static void Error(Args &&...args) { s_logger->error(std::forward<Args>(args)...); }

private:
    static std::shared_ptr<spdlog::logger> s_logger;
};
}// namespace sigmax
