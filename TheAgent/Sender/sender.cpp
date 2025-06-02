// Sender.cpp : Defines the functions for the static library.
//
#include "pch.h"
#include "framework.h"
#include <thread>
#include <iostream>
#include "sender.h"
#define CURL_STATICLIB
#include <curl/curl.h>
#include "utility.h"
#include "Logger.h"


Sender::Sender(EventQueueManager& eventQueueManager, const std::string& url)
    : m_eventQueueManager(eventQueueManager), running_(false) {

    
    h_StopSenderEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

}

Sender::~Sender() {
    Stop();
}
void Sender::Start(const unsigned int& Iteration, const unsigned int& MaxEvent, const std::wstring& localpath, const std::wstring& url) {
    // Set the log format from settings
    interval_ = Iteration;
	max_event_ = MaxEvent;
    path_ = localpath;
    url_ = url;

    LOGW(Logger::INFO, Utility::FormatString(L"Send interval. Interval: %u", interval_));
    LOGW(Logger::INFO, Utility::FormatString(L"Send max events per interval. max events: %u", max_event_));
    LOGW(Logger::INFO, Utility::FormatString(L"Send localpath. filepath: %ls", path_.c_str()));
    LOGW(Logger::INFO, Utility::FormatString(L"Send url path. url: %ls", url_.c_str()));

    running_ = true;
    std::thread(&Sender::Run, this).detach();
}

void Sender::Stop() {
    SetEvent(h_StopSenderEvent);
    running_ = false;
}

void Sender::Run() {

    while (running_) {
        unsigned int current_count = 0;
        unsigned int pop_count = 0;

        pop_count = m_eventQueueManager.Size();
		if (pop_count > max_event_) {
            pop_count = max_event_;
		}

        while (current_count < pop_count && !m_eventQueueManager.IsEmpty()) {
            std::wstring item = m_eventQueueManager.Peek();

            if (AppendJsonToArray(item)) {
                // Remove the item on sent success
                m_eventQueueManager.Pop();
                current_count++;
            }
        }

        if (url_.length() <= 0) {

            write2file_(path_, jsonArray_);
            // clear json if success
            jsonArray_.clear();
            jsonArray_.shrink_to_fit();
            jsonArray_ = L"[]";
        }

        DWORD status = ERROR_SUCCESS;

		DWORD wait = WaitForSingleObject(h_StopSenderEvent, interval_ * 60000); // Convert minutes to milliseconds

        if (wait == WAIT_OBJECT_0)
        {
            break;
        }
        else if (wait == WAIT_TIMEOUT)
        {
            status = ERROR_SUCCESS;
        }
        else
        {
            LOG(Logger::ERRORS, Utility::FormatString(
                L"Failed to wait operation on m_stopEvent handle. Error: %lu.",
                GetLastError()));
            break;
        }

    }
}

// Callback function to write the response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool Sender::send_old(const std::wstring& logname, const std::wstring& data) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    std::string content = Utility::wstring_to_string(data);
    std::string filename = Utility::wstring_to_string(logname);

    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Initialize curl
    curl = curl_easy_init();
    if (curl) {
        // Set the URL for the GET request
        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.152.130:8000/upload/store/");

        // Set the write callback function
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        //curl_easy_setopt(curl, CURLOPT_POSTFIELDS, std::string(data.begin(), data.end()).c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content.c_str());

        // Set the content type to application/x-www-form-urlencoded
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        // Add the filename to the headers
        std::string filenameHeader = "Filename: " + filename;
        headers = curl_slist_append(headers, filenameHeader.c_str());

        // Set the custom headers
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Perform the request, res will get the return code
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return false;
        }
        else {
            // Output the response data
            std::cout << "Response data:\n" << readBuffer << std::endl;
        }

        // Clean up the curl headers
        curl_slist_free_all(headers);

        // Clean up the curl environment
        curl_easy_cleanup(curl);
    }
    else {
        std::cerr << "curl_easy_init() failed" << std::endl;
        curl_global_cleanup();
        return false;
    }

    // Clean up the CURL global environment
    curl_global_cleanup();

    return true;
}

bool Sender::write2file_(const std::wstring& folderPath, const std::wstring& jsonData) {
    try {
        std::string utf8Data = wstringToUtf8(jsonData);
        if (utf8Data.length() < 3) {
            LOGW(Logger::ERRORS, Utility::FormatString(L"Invalid or too short JSON data."));
            return false;
        }

        std::wstring cleanFolder = sanitizeFolderPath(folderPath);
        std::wstring fullPath = cleanFolder + L"\\" + generateTimestampedFilename();

        FILE* fp = nullptr;
        errno_t err = _wfopen_s(&fp, fullPath.c_str(), L"wb");

        if (err != 0 || fp == nullptr) {
            wchar_t errMsg[256] = {};
            char errBuf[256] = {};
            strerror_s(errBuf, sizeof(errBuf), err);
            MultiByteToWideChar(CP_UTF8, 0, errBuf, -1, errMsg, sizeof(errMsg) / sizeof(wchar_t));
            LOGW(Logger::ERRORS, Utility::FormatString(
                L"Failed to open file: %ls, errno: %d, message: %ls",
                fullPath.c_str(), err, errMsg));
            return false;
        }

        size_t written = fwrite(utf8Data.data(), 1, utf8Data.size(), fp);
        if (written != utf8Data.size()) {
            wchar_t errMsg[256] = {};
            char errBuf[256] = {};
            strerror_s(errBuf, sizeof(errBuf), errno);
            MultiByteToWideChar(CP_UTF8, 0, errBuf, -1, errMsg, sizeof(errMsg) / sizeof(wchar_t));
            LOGW(Logger::ERRORS, Utility::FormatString(
                L"Write failed to file: %ls, written: %zu, expected: %zu, errno: %d, message: %ls",
                fullPath.c_str(), written, utf8Data.size(), errno, errMsg));
            fclose(fp);
            return false;
        }

        if (fclose(fp) != 0) {
            wchar_t errMsg[256] = {};
            char errBuf[256] = {};
            strerror_s(errBuf, sizeof(errBuf), errno);
            MultiByteToWideChar(CP_UTF8, 0, errBuf, -1, errMsg, sizeof(errMsg) / sizeof(wchar_t));
            LOGW(Logger::ERRORS, Utility::FormatString(
                L"Failed to close file: %ls, errno: %d, message: %ls",
                fullPath.c_str(), errno, errMsg));
            return false;
        }

        return true;
    }
    catch (const std::exception& ex) {
        std::wstring msg;
        int len = MultiByteToWideChar(CP_UTF8, 0, ex.what(), -1, nullptr, 0);
        if (len > 0) {
            msg.resize(len - 1);
            MultiByteToWideChar(CP_UTF8, 0, ex.what(), -1, &msg[0], len);
        }
        LOGW(Logger::ERRORS, Utility::FormatString(L"Exception in write2file_: %ls", msg.c_str()));
        return false;
    }
    catch (...) {
        LOGW(Logger::ERRORS, Utility::FormatString(L"Unknown exception in write2file_"));
        return false;
    }
}

std::wstring Sender::generateTimestampedFilename() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
    errno_t err = localtime_s(&tm, &t);

    if (err != 0) {
        LOGW(Logger::ERRORS, Utility::FormatString(
            L"localtime_s failed with errno: %d", err));
        return L"output_invalid_time.json";
    }

    wchar_t buffer[64];
    if (0 == wcsftime(buffer, sizeof(buffer) / sizeof(wchar_t), L"output_%Y%m%d_%H%M%S.json", &tm)) {
        LOGW(Logger::ERRORS, Utility::FormatString(L"wcsftime failed to format time."));
        return L"output_invalid_time.json";
    }

    return std::wstring(buffer);
}

std::wstring Sender::sanitizeFolderPath(const std::wstring& path) {
    std::wstring sanitizedPath = path;

    // Check for folder traversal using simple substring search
    if (sanitizedPath.find(L"..\\") != std::wstring::npos || sanitizedPath.find(L"../") != std::wstring::npos) {
        LOGW(Logger::ERRORS, Utility::FormatString(L"sanitizeFolderPath detected folder traversal attempt: %ls", sanitizedPath.c_str()));
        sanitizedPath.clear(); // Force fallback
    }

    // Check if path is empty or doesn't exist
    if (sanitizedPath.empty() || GetFileAttributesW(sanitizedPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (sanitizedPath.empty()) {
            LOGW(Logger::ERRORS, Utility::FormatString(L"sanitizeFolderPath received an empty or invalid path."));
        }
        else {
            LOGW(Logger::ERRORS, Utility::FormatString(L"sanitizeFolderPath received a non-existent path: %ls", sanitizedPath.c_str()));
        }

        // Get the executable's directory
        wchar_t exePath[MAX_PATH] = { 0 };
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);

        // Remove the executable file name to get the directory
        wchar_t* lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) {
            *lastSlash = L'\0'; // Truncate to directory
        }

        sanitizedPath = exePath;

        LOGW(Logger::INFO, Utility::FormatString(L"Using executable directory as fallback path: %ls", sanitizedPath.c_str()));
    }

    // Trim trailing slash or backslash
    size_t len = sanitizedPath.length();
    if (len > 0 && (sanitizedPath[len - 1] == L'\\' || sanitizedPath[len - 1] == L'/')) {
        sanitizedPath.erase(len - 1);
        LOGW(Logger::INFO, Utility::FormatString(L"Trimming trailing slash from folder path: %ls", sanitizedPath.c_str()));
    }

    return sanitizedPath;
}

std::string Sender::wstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) {
        LOGW(Logger::INFO, Utility::FormatString(L"wstringToUtf8 received an empty string."));
        return {};
    }

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        LOGW(Logger::ERRORS, Utility::FormatString(L"WideCharToMultiByte failed to calculate size."));
        return {};
    }

    std::string result(size_needed, 0);
    int bytes_written = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &result[0], size_needed, nullptr, nullptr);
    if (bytes_written != size_needed) {
        LOGW(Logger::ERRORS, Utility::FormatString(
            L"WideCharToMultiByte mismatch: expected %d bytes, wrote %d", size_needed, bytes_written));
        return {};
    }

    return result;
}


// Append a new JSON element to the existing JSON array
bool Sender::AppendJsonToArray(const std::wstring& newElement) {
    try {
        if (jsonArray_ == L"[]") {
            jsonArray_ = L"[" + newElement + L"]";
        }
        else {
            // Insert before the closing bracket
            jsonArray_.insert(jsonArray_.size() - 1, L"," + newElement);
        }

    }
    catch (const std::exception& ex) {
        LOG(Logger::ERRORS, ex.what());
        return false;
    }

    return true;
}
