#ifndef SIRPG_LOGGER_HPP
#define SIRPG_LOGGER_HPP

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

namespace sirpg::core {

class Logger {
public:
    static void init() {
        if (!s_logger) {
            s_logger = spdlog::stdout_color_mt("ENGINE");
            s_logger->set_level(spdlog::level::info);
            s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
        }
    }

    static std::shared_ptr<spdlog::logger>& get() {
        if (!s_logger) {
            init();
        }
        return s_logger;
    }

private:
    inline static std::shared_ptr<spdlog::logger> s_logger{nullptr};
};

} // namespace sirpg::core

#define LOG_TRACE(...) ::sirpg::core::Logger::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::sirpg::core::Logger::get()->debug(__VA_ARGS__)
#define LOG_INFO(...)  ::sirpg::core::Logger::get()->info(__VA_ARGS__)
#define LOG_WARN(...)  ::sirpg::core::Logger::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::sirpg::core::Logger::get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::sirpg::core::Logger::get()->critical(__VA_ARGS__)

#endif // SIRPG_LOGGER_HPP
