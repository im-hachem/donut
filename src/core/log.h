#pragma once

#include "core/memory.h"

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <string_view>

namespace Donut
{
    enum class LogLevel
    {
        trace = 0,
        Info  = 2,
        Warn  = 3,
        Err   = 4,
        Fatal = 5
    };

    class Logger
    {
    public:
        Logger();
        ~Logger();

        static auto init() -> void;
        static auto shutdown() -> void;

        static auto get_logger() -> Ref<Logger>;

        template<typename... Args>
        static auto trace(const std::string_view& format, const Args&... args) -> void
        {
            auto logger = get_logger();
            if (logger) logger->log_message(LogLevel::trace, format, args...);
        }

        template<typename... Args>
        static auto info(const std::string_view& format, const Args&... args) -> void
        {
            auto logger = get_logger();
            if (logger) logger->log_message(LogLevel::Info, format, args...);
        }

        template<typename... Args>
        static auto warn(const std::string_view& format, const Args&... args) -> void
        {
            auto logger = get_logger();
            if (logger) logger->log_message(LogLevel::Warn, format, args...);
        }

        template<typename... Args>
        static auto error(const std::string_view& format, const Args&... args) -> void
        {
            auto logger = get_logger();
            if (logger) logger->log_message(LogLevel::Err, format, args...);
        }

        template<typename... Args>
        static auto fatal(const std::string_view& format, const Args&... args) -> void
        {
            auto logger = get_logger();
            if (logger) logger->log_message(LogLevel::Fatal, format, args...);
        }

        template<typename... Args>
        auto log_message(LogLevel level, const std::string_view& format, const Args&... args) -> void
        {
            if (level < m_log_level)
                return;

            std::lock_guard<std::mutex> lock(m_mutex);
            std::string message = format_string(format, args...);
            std::string full_message = get_time_stamp() + " [" + get_log_level_string(level) + "] " + message;

            if (m_console_output)
            {
                set_console_color(level);
                std::cout << full_message << std::endl;
                reset_console_color();
            }

            if (m_file_output && m_log_file.is_open())
            {
                m_log_file << full_message << std::endl;
                m_log_file.flush();
            }
        }

        template<typename... Args>
        auto format_string(const std::string_view& format, const Args&... args) -> std::string
        {
            std::string result = format.data();
            std::vector<std::string> arg_strings = { to_string(args)... };

            size_t arg_index = 0;
            size_t pos = 0;

            while ((pos = result.find("{}", pos)) != std::string::npos && arg_index < arg_strings.size())
            {
                result.replace(pos, 2, arg_strings[arg_index]);
                pos += arg_strings[arg_index].length();
                arg_index++;
            }

            return result;
        }

        template<typename T>
        auto to_string(const T& value) -> std::string
        {
            std::stringstream ss;
            ss << value;
            return ss.str();
        }

        auto set_log_level(LogLevel level) -> void      { m_log_level      = level;  }
        auto enable_console_output(bool enable) -> void { m_console_output = enable; }
        auto enable_file_output(bool enable) -> void    { m_file_output    = enable; }
        auto set_log_file(const std::string& filename) -> void;

        auto get_time_stamp() -> std::string;
        auto get_log_level_string(LogLevel level) -> std::string;
        auto set_console_color(LogLevel level) -> void;
        auto reset_console_color() -> void;

    private:
        LogLevel      m_log_level;
        bool          m_console_output;
        bool          m_file_output;
        std::ofstream m_log_file;
        std::mutex    m_mutex;

        static Ref<Logger> s_logger;
    };
}

#if defined(DONUT_DEBUG)
    #define DONUT_TRACE(format, ...)    ::Donut::Logger::trace(format, ##__VA_ARGS__)
    #define DONUT_INFO(format, ...)     ::Donut::Logger::info(format, ##__VA_ARGS__)
    #define DONUT_WARN(format, ...)     ::Donut::Logger::warn(format, ##__VA_ARGS__)
    #define DONUT_ERROR(format, ...)    ::Donut::Logger::error(format, ##__VA_ARGS__)
    #define DONUT_FATAL(format, ...)    ::Donut::Logger::fatal(format, ##__VA_ARGS__)
#else
    #define DONUT_TRACE(format, ...)    {}
    #define DONUT_INFO(format, ...)     {}
    #define DONUT_WARN(format, ...)     {}
    #define DONUT_ERROR(format, ...)    {}
    #define DONUT_FATAL(format, ...)    {}
#endif
