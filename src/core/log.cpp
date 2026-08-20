#include "log.h"

#include <filesystem>

#if defined(DONUT_WINDOWS)
    #include <windows.h>
#endif

namespace Donut
{
    Ref<Logger> Logger::s_logger;

    auto Logger::init() -> void
    {
        s_logger = create_ref<Logger>();
        s_logger->set_log_level(LogLevel::Info);
        s_logger->enable_console_output(true);
        s_logger->enable_file_output(true);
        s_logger->set_log_file("logs/donut.log");

        s_logger->log_message(LogLevel::Info, "Logging system initialized");
    }

    auto Logger::shutdown() -> void
    {
        if (s_logger)
            s_logger->log_message(LogLevel::Info, "Shutting down logging system");
        s_logger.reset();
    }

    auto Logger::get_logger() -> Ref<Logger>
    {
        return s_logger;
    }

    Logger::Logger()
        : m_log_level(LogLevel::Info),
          m_console_output(true),
          m_file_output(false) { }

    Logger::~Logger()
    {
        if (m_log_file.is_open())
            m_log_file.close();
    }

    auto Logger::set_log_file(const std::string& filename) -> void
    {
        std::filesystem::path log_path(filename);
        std::filesystem::create_directories(log_path.parent_path());

        m_log_file.open(filename, std::ios::app);
        if (!m_log_file.is_open())
            std::cerr << "Failed to open log file: " << filename << std::endl;
    }

    auto Logger::get_time_stamp() -> std::string
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

        return ss.str();
    }

    auto Logger::get_log_level_string(LogLevel level) -> std::string
    {
        switch (level)
        {
            case LogLevel::trace: return "TRACE";
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Err:   return "ERROR";
            case LogLevel::Fatal: return "FATAL";
            default:              return "UNKNOWN";
        }
    }

    auto Logger::set_console_color(LogLevel level) -> void
    {
#if defined(DONUT_WINDOWS)
        HANDLE h_console = GetStdHandle(STD_OUTPUT_HANDLE);
        WORD color;

        switch (level) {
            case LogLevel::trace:
                color = FOREGROUND_RED   |
                        FOREGROUND_GREEN |
                        FOREGROUND_BLUE;
                break;
            case LogLevel::Info:
                color = FOREGROUND_GREEN |
                        FOREGROUND_INTENSITY;
                break;
            case LogLevel::Warn:
                color = FOREGROUND_RED   |
                        FOREGROUND_GREEN |
                        FOREGROUND_INTENSITY;
                break;
            case LogLevel::Err:
                color = FOREGROUND_RED |
                        FOREGROUND_INTENSITY;
                break;
            case LogLevel::Fatal:
                color = FOREGROUND_RED  |
                        FOREGROUND_BLUE |
                        FOREGROUND_INTENSITY;
                break;
            default:
                color = FOREGROUND_RED   |
                        FOREGROUND_GREEN |
                        FOREGROUND_BLUE;
                break;
        }

        SetConsoleTextAttribute(h_console, color);
#else
        const char* color_code;
        switch (level) {
            case LogLevel::trace:
                color_code = "\033[37m";
                break;
            case LogLevel::Info:
                color_code = "\033[32;1m";
                break;
            case LogLevel::Warn:
                color_code = "\033[33;1m";
                break;
            case LogLevel::Err:
                color_code = "\033[31;1m";
                break;
            case LogLevel::Fatal:
                color_code = "\033[35;1m";
                break;
            default:
                color_code = "\033[37m";
                break;
        }
        std::cout << color_code;
#endif
    }

    auto Logger::reset_console_color() -> void
    {
#if defined(DONUT_WINDOWS)
        HANDLE h_console = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(h_console, FOREGROUND_RED   |
                                          FOREGROUND_GREEN |
                                          FOREGROUND_BLUE);
#else
        std::cout << "\033[0m";
#endif
    }
}
