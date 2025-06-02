#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

class Logger {
public:
    // Log levels
    enum LogLevel { INFO, WARNING, ERRORS };

    // Singleton instance getter
    static Logger& getInstance();

    // Log a message with timestamp, source file, line number, and log level
    void log(LogLevel level, const std::string& message, const char* file, int line);

    void log(LogLevel level, const std::wstring& message, const char* file, int line);
    void logW(LogLevel level, const std::wstring& message, const char* file, int line);

    // Set the log file (optional)
    void setLogFile(const std::string& filename);

    void printInstanceAddress();  // New method to print the address of the instance

private:
    Logger();
    ~Logger(); 

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex logMutex;  // Mutex for thread-safety
    std::ofstream logFile;  // Log file stream

    std::string getCurrentTime();
    std::string logLevelToString(LogLevel level);
    std::string wstring_to_string(const std::wstring& wstr);

};

#define LOG(level, message) Logger::getInstance().log(level, message, __FILE__, __LINE__)
#define LOGW(level, message) Logger::getInstance().logW(level, message, __FILE__, __LINE__)


