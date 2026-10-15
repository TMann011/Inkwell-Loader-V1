#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <Windows.h>

namespace Inkwell {

enum class LogLevel {
    Info,
    Success,
    Warning,
    Error,
    Stealth
};

struct LogEntry {
    std::string timestamp;
    LogLevel level;
    std::string message;
};

class Logger {
public:
    static Logger& Get();

    void Init(const std::string& logFilename = "inkwell_loader.log");
    void Shutdown();

    void Log(LogLevel level, const char* fmt, ...);
    void LogW(LogLevel level, const wchar_t* fmt, ...);

    std::vector<LogEntry> GetEntries();
    void Clear();

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string CurrentTimestamp();

    std::mutex m_mutex;
    std::vector<LogEntry> m_entries;
    std::ofstream m_file;
    bool m_initialized = false;
};

} // namespace Inkwell
