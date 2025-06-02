//
// Copyright (c) 
// Licensed under the MIT license.
//

#pragma once
#include "EventQueueManager.h"

class EndpointMonitor final
{
public:

    EndpointMonitor() = delete;

    EndpointMonitor(
        _In_ EventQueueManager& eventQueueManager,
        _In_ std::wstring CustomLogFormat
        );

    ~EndpointMonitor();

    static std::wstring EventFieldsMapping(_In_ std::wstring eventField, _In_ void* pLogEntryData);

private:
    static constexpr int EVENT_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS = 5 * 1000;
    static constexpr int EVENT_ARRAY_SIZE = 10;

    std::wstring m_logFormat;
    std::wstring m_customLogFormat;
    EventQueueManager& m_eventQueueManager;

    struct EventLogEntry {
        std::wstring source;
        std::wstring eventSource;
        std::wstring eventTime;
        std::wstring eventChannel;
        std::wstring eventLevel;
        UINT16 eventId;
        std::wstring eventMessage;
    };

    //
    // Signaled by destructor to request the spawned thread to stop.
    //
    HANDLE m_stopEvent;

    //
    // Handle to an event subscriber thread.
    //
    HANDLE m_eventMonitorThread;

    std::vector<wchar_t> m_eventMessageBuffer;

    DWORD StartEventMonitor();

    static DWORD StartEventMonitorStatic(
        _In_ LPVOID Context
        );

    DWORD EnumerateResults(
        _In_ EVT_HANDLE ResultsHandle
        );

    DWORD PrintEvent(
        _In_ const HANDLE& EventHandle
        );

    std::map<std::wstring, std::wstring> parseSettings(const std::wstring& output);

    std::wstring toJson(const std::map<std::wstring, std::wstring>& settings);

    int __get();
};
