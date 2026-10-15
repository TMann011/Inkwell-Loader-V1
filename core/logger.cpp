#include "logger.h"
#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Inkwell {

Logger& Logger::Get() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    Shutdown();
}

void Logger::Init(const std::string& logFilename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        m_file.open(logFilename, std::ios::out | std::ios::app);
        m_initialized = true;
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
    m_initialized = false;
}

std::string Logger::CurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm bt{};
    localtime_s(&bt, &in_time_t);

    std::ostringstream ss;
    ss << std::put_time(&bt, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void Logger::Log(LogLevel level, const char* fmt, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::string timestamp = CurrentTimestamp();
    std::string text(buffer);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.push_back({ timestamp, level, text });

    if (m_entries.size() > 500) {
        m_entries.erase(m_entries.begin(), m_entries.begin() + 50);
    }

    if (m_file.is_open()) {
        const char* tag = "[INFO]";
        switch (level) {
        case LogLevel::Success: tag = "[OK]  "; break;
        case LogLevel::Warning: tag = "[WARN]"; break;
        case LogLevel::Error:   tag = "[ERR] "; break;
        case LogLevel::Stealth: tag = "[CLOAK]"; break;
        default: break;
        }
        m_file << "[" << timestamp << "] " << tag << " " << text << "\n";
        m_file.flush();
    }
}

void Logger::LogW(LogLevel level, const wchar_t* fmt, ...) {
    wchar_t buffer[2048];
    va_list args;
    va_start(args, fmt);
    vswprintf(buffer, sizeof(buffer) / sizeof(wchar_t), fmt, args);
    va_end(args);

    char narrow[2048];
    size_t converted = 0;
    wcstombs_s(&converted, narrow, sizeof(narrow), buffer, _TRUNCATE);

    Log(level, "%s", narrow);
}

std::vector<LogEntry> Logger::GetEntries() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries;
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

} // namespace Inkwell
