// Logger.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "framework.h"
#include "Logger.h"
#include <windows.h>
#include <iostream>
#include <string>

// Singleton instance
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

// Get current timestamp in string format
std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm;
    localtime_s(&local_tm, &time_t_now);  // Use localtime_s to fill local_tm

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S"); // Use &local_tm
    return oss.str();
}

// Convert LogLevel to string
std::string Logger::logLevelToString(LogLevel level) {
    switch (level) {
    case INFO: return "INFO";
    case WARNING: return "WARNING";
    case ERRORS: return "ERROR";
    default: return "UNKNOWN";
    }
}

// Main logging function
void Logger::log(LogLevel level, const std::string& message, const char* file, int line) {
    std::lock_guard<std::mutex> guard(logMutex);  // Lock the mutex for thread-safety

    // Get current time and log level as string
    std::string timeStr = getCurrentTime();
    std::string levelStr = logLevelToString(level);

    // Log to console
    std::cout << "[" << timeStr << "] [" << levelStr << "] (" << file << ":" << line << ") " << message << std::endl;

    // Optionally, log to a file
    if (logFile.is_open()) {
        logFile << "[" << timeStr << "] [" << levelStr << "] (" << file << ":" << line << ") " << message << std::endl;
    }
    else {
        std::cerr << "Failed to open log file: " << std::endl;
    }
}

void Logger::log(LogLevel level, const std::wstring& wmessage, const char* file, int line) {
    std::lock_guard<std::mutex> guard(logMutex);  // Lock the mutex for thread-safety

    // Get current time and log level as string
    std::string timeStr = getCurrentTime();
    std::string levelStr = logLevelToString(level);
    std::string message = wstring_to_string(wmessage);

    // Log to console
    std::cout << "[" << timeStr << "] [" << levelStr << "] (" << file << ":" << line << ") " << message << std::endl;

    // Optionally, log to a file
    if (logFile.is_open()) {
        logFile << "[" << timeStr << "] [" << levelStr << "] (" << file << ":" << line << ") " << message << std::endl;
    }
    else {
        std::cerr << "Failed to open log file: " << std::endl;
    }
}

void Logger::logW(LogLevel level, const std::wstring& wmessage, const char* file, int line) {
    std::lock_guard<std::mutex> guard(logMutex);  // Lock the mutex for thread-safety

    // Get current time and log level as string
    std::string timeStr = getCurrentTime();
    std::string levelStr = logLevelToString(level);
    std::string message = wstring_to_string(wmessage);

    // Log to console
    std::cout << "[" << timeStr << "] [" << levelStr << "] (" << file << ":" << line << ") " << message << std::endl;

    // Optionally, log to a file
    if (logFile.is_open()) {
        logFile << "[" << timeStr << "] [" << levelStr << "] (" << file << ":" << line << ") " << message << std::endl;
    }
    else {
        std::cerr << "Failed to open log file: " << std::endl;
    }
}

std::string Logger::wstring_to_string(const std::wstring& wstr) {
    // First, get the required size for the destination buffer
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);

    // Allocate a buffer to hold the converted string
    std::string str(size_needed, 0);

    // Perform the conversion to UTF-8
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);

    return str;
}

// Set the log file
void Logger::setLogFile(const std::string& filename) {
    logFile.open(filename, std::ios::out | std::ios::binary | std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
    }
    else {
        //To explicitly specify UTF-8 encoding, write the UTF-8 BOM at the beginning of the file
        logFile.put(0xEF);
        logFile.put(0xBB);
        logFile.put(0xBF);
    }
}

void Logger::printInstanceAddress() {
    std::cout << "Logger instance address: " << &getInstance() << std::endl;
}

// Constructor
Logger::Logger() {

}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}