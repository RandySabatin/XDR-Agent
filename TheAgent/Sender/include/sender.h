#pragma once
#include <windows.h>
#include <string>
#include "EventQueueManager.h"
//#include "LoggerSettings.h"
#include <ctime>
#include <codecvt>
#include <locale>
#include <cstdio>
#include <cstring>
#include <cerrno>

class Sender {
public:
    Sender(EventQueueManager& eventQueueManager, const std::string& url);
    void Start(const unsigned int& Iteration, const unsigned int& MaxEvent, const std::wstring& localpath, const std::wstring& url);
    void Stop();

    ~Sender();
    HANDLE h_StopSenderEvent;

private:
    
    bool write2file_(const std::wstring& folderPath, const std::wstring& jsonData);
    std::wstring generateTimestampedFilename();
    std::wstring sanitizeFolderPath(const std::wstring& path);
    std::string wstringToUtf8(const std::wstring& wstr);
    bool AppendJsonToArray(const std::wstring& newElement);
    std::wstring jsonArray_ = L"[]";

    bool send_(const std::wstring& logname, const std::wstring& data);
    bool send_old(const std::wstring& logname, const std::wstring& data);
    void Run();
    

    EventQueueManager& m_eventQueueManager;
    std::wstring url_;
    std::wstring path_;
    unsigned int interval_ = 5;
	unsigned int max_event_ = 500;

    bool running_;
};
