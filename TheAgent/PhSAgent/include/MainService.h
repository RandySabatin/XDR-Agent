/****************************** Module Header ******************************\
* Module Name:  MainService.h
* Project:      phSAgent
\***************************************************************************/

#pragma once
//#include <Windows.h>  // For HMODULE
#include "ServiceBase.h"
#include <string>
#include "LoggerSettings.h"
#include "pch.h"


class MainService : public CServiceBase
{
public:

    MainService(PWSTR pszServiceName, 
        BOOL fCanStop = TRUE, 
        BOOL fCanShutdown = TRUE, 
        BOOL fCanPauseContinue = FALSE);
    virtual ~MainService(void);

protected:

    virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv);
    virtual void OnStop();

    void ServiceWorkerThread(void);

    void StartMonitors(_In_ LoggerSettings& settings);

private:

    HANDLE m_hStoppingEvent;
    HANDLE m_hStoppedEvent;
    std::unique_ptr<EventMonitor> g_eventMon;
    std::unique_ptr<EndpointMonitor> g_endpointMon;
    std::unique_ptr<Sender> g_sender;
    std::wstring logFormat;
    EventQueueManager m_eventQueueManager;

    void InitializeEventLogMonitor(
        std::shared_ptr<SourceEventLog> sourceEventLog,
        std::vector<EventLogChannel>& eventChannels,
        bool& eventMonMultiLine,
        bool& eventMonStartAtOldestRecord,
        std::wstring& eventCustomLogFormat);

    void CreateEventMonitor(
        std::vector<EventLogChannel>& eventChannels,
        bool eventMonMultiLine,
        bool eventMonStartAtOldestRecord,
        const std::wstring& eventCustomLogFormat);

};



// Define the function pointer type for the log_message function
typedef void (*LogFunction)(const std::wstring&);

// Declare global variables: the DLL handle (hModule1) and the function pointer (logFunc)
extern HMODULE hModule;       // Handle to the loaded DLL
extern LogFunction logFunc;     // Function pointer to log_message