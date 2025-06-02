/****************************** Module Header ******************************\
* Project:      PhSAgent

\***************************************************************************/

#pragma region Includes
#include <iostream>
#include "MainService.h"
#include "ThreadPool.h"
#include "logger.h"
#include "ConfigFileParser.h"
#include "LoggerSettings.h"
#include "EventQueueManager.h"
#include "Sender.h"

//#include "pch.h"


#pragma endregion


MainService::MainService(PWSTR pszServiceName, 
                               BOOL fCanStop, 
                               BOOL fCanShutdown, 
                               BOOL fCanPauseContinue)
: CServiceBase(pszServiceName, fCanStop, fCanShutdown, fCanPauseContinue)
{
    LOG(Logger::INFO, "MainService::MainService");
    // Create a manual-reset event that is not signaled at first to indicate 
    // the stopped signal of the service.
    m_hStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hStoppingEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_eventMon = nullptr;
    g_endpointMon = nullptr;
    
    if (m_hStoppedEvent == NULL)
    {
        throw GetLastError();
    }
}


MainService::~MainService(void)
{
    if (m_hStoppedEvent)
    {
        CloseHandle(m_hStoppedEvent);
        m_hStoppedEvent = NULL;
    }
}


//
//   FUNCTION: MainService::OnStart(DWORD, LPWSTR *)
//
//   PURPOSE: The function is executed when a Start command is sent to the 
//   service by the SCM or when the operating system starts (for a service 
//   that starts automatically). It specifies actions to take when the 
//   service starts. In this code sample, OnStart logs a service-start 
//   message to the Application log, and queues the main service function for 
//   execution in a thread pool worker thread.
//
//   PARAMETERS:
//   * dwArgc   - number of command line arguments
//   * lpszArgv - array of command line arguments
//
//   NOTE: A service application is designed to be long running. Therefore, 
//   it usually polls or monitors something in the system. The monitoring is 
//   set up in the OnStart method. However, OnStart does not actually do the 
//   monitoring. The OnStart method must return to the operating system after 
//   the service's operation has begun. It must not loop forever or block. To 
//   set up a simple monitoring mechanism, one general solution is to create 
//   a timer in OnStart. The timer would then raise events in your code 
//   periodically, at which time your service could do its monitoring. The 
//   other solution is to spawn a new thread to perform the main service 
//   functions, which is demonstrated in this code sample.
//
void MainService::OnStart(DWORD dwArgc, LPWSTR *lpszArgv)
{
    // Log a service start message to the Application log.
    WriteEventLogEntry(L"CppWindowsService in OnStart", 
        EVENTLOG_INFORMATION_TYPE);

    g_sender = std::make_unique<Sender>(
        m_eventQueueManager,
        "sample"
    );
    // Queue the main service function for execution in a worker thread.
    CThreadPool::QueueUserWorkItem(&MainService::ServiceWorkerThread, this);
}


//
//   FUNCTION: Main::ServiceWorkerThread(void)
//
//   PURPOSE: The method performs the main function of the service. It runs 
//   on a thread pool worker thread.
//
void MainService::ServiceWorkerThread(void)
{
    PWCHAR configFileName = (PWCHAR)DEFAULT_CONFIG_FILENAME;

    wchar_t path[MAX_PATH];
    // Get the full path of the executable
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        LOG(Logger::ERRORS, L"Failed to get executable path.");
        throw GetLastError();
    }
    std::wstring fullPath(path);
    // Remove the executable file name to get the directory
    size_t pos = fullPath.find_last_of(L"\\/");
    std::wstring dirPath = (pos != std::wstring::npos) ? fullPath.substr(0, pos + 1) : L"";
    // Append "LogMonitorConfig.json"
    std::wstring configPath = dirPath + DEFAULT_CONFIG_FILENAME;
	// Convert to PWCHAR
	configFileName = (PWCHAR)configPath.c_str();

    LoggerSettings settings;
    //read the config file
    bool configFileReadSuccess = OpenConfigFile(configFileName, settings);

    //start the monitors
    if (configFileReadSuccess)
    {
		m_eventQueueManager.SetMaxSize(settings.Max_queue_events);
        g_sender->Start(settings.SendInterval, settings.Max_send_events, settings.SendLocalPath, settings.SendURLPath);

        StartMonitors(settings);
    }
    else {
        LOG(Logger::ERRORS, L"Invalid configuration file.");
    }

    HANDLE evnt_EventMonitor = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    //EventMonitor 
    /*
    Lib1 lib1(evnt_EventMonitor);

    //lib1.start();

    while (true) {
        HANDLE handles[3] = { event1, event2, event3 };
        DWORD waitResult = WaitForMultipleObjects(3, handles, FALSE, INFINITE);

        switch (waitResult) {
        case WAIT_OBJECT_0: // event1 signaled
            std::cout << "Main process: Event 1 signaled\n";
            ResetEvent(event1);
            break;
        case WAIT_FAILED:
            std::cerr << "Wait failed: " << GetLastError() << "\n";
            return 1;
        }
    }

    // Clean up (this part is unreachable in this example)
    CloseHandle(event1);

    */
    if (WaitForSingleObject(m_hStoppingEvent, INFINITE) != WAIT_OBJECT_0)
    {
        throw GetLastError();
    }

    // Signal the stopped event.
    SetEvent(m_hStoppedEvent);
}


//
//   FUNCTION: MainService::OnStop(void)
//
//   PURPOSE: The function is executed when a Stop command is sent to the 
//   service by SCM. It specifies actions to take when a service stops 
//   running. In this code sample, OnStop logs a service-stop message to the 
//   Application log, and waits for the finish of the main service function.
//
//   COMMENTS:
//   Be sure to periodically call ReportServiceStatus() with 
//   SERVICE_STOP_PENDING if the procedure is going to take long time. 
//
void MainService::OnStop()
{
    // Log a service stop message to the Application log.
    WriteEventLogEntry(L"CppWindowsService in OnStop", 
        EVENTLOG_INFORMATION_TYPE);
    g_sender->Stop();

    // Indicate that the service is stopping and wait for the finish of the 
    // main service function (ServiceWorkerThread).

    // Signal the stopping event.
    SetEvent(m_hStoppingEvent);

    if (WaitForSingleObject(m_hStoppedEvent, INFINITE) != WAIT_OBJECT_0)
    {
        throw GetLastError();
    }
}

/// <summary>
/// Start the monitors by delegating to the helper functions based on log source type
/// </summary>
/// <param name="settings">The LoggerSettings object containing configuration</param>
void MainService::StartMonitors(_In_ LoggerSettings& settings)
{
    // Vectors to store the event log channels and ETW providers
    std::vector<EventLogChannel> eventChannels;
    std::vector<ETWProvider> etwProviders;

    bool eventMonMultiLine;
    bool eventMonStartAtOldestRecord;
    bool etwMonMultiLine;

    // Set the log format from settings
    logFormat = settings.LogFormat;

    // Custom log formats for the different sources
    std::wstring eventCustomLogFormat;
    std::wstring etwCustomLogFormat;
    std::wstring processCustomLogFormat;

    // Iterate through each log source defined in the settings
    for (auto source : settings.Sources)
    {
        switch (source->Type)
        {
        case LogSourceType::EventLog:
        {
            std::shared_ptr<SourceEventLog> sourceEventLog =
                std::reinterpret_pointer_cast<SourceEventLog>(source);
            InitializeEventLogMonitor(
                sourceEventLog,
                eventChannels,
                eventMonMultiLine,
                eventMonStartAtOldestRecord,
                eventCustomLogFormat
            );
            break;
        }
        case LogSourceType::File:
        {
            //disable first
            /*
            std::shared_ptr<SourceFile> sourceFile =
                std::reinterpret_pointer_cast<SourceFile>(source);
            InitializeFileMonitor(sourceFile);
            */
            break;
        }
        case LogSourceType::ETW:
        {
            //disable first
            /*
            std::shared_ptr<SourceETW> sourceETW =
                std::reinterpret_pointer_cast<SourceETW>(source);
            InitializeEtwMonitor(
                sourceETW,
                etwProviders,
                etwMonMultiLine,
                etwCustomLogFormat
            );
            */
            break;
        }
        case LogSourceType::Process:
        {
            //disable first
            /*
            std::shared_ptr<SourceProcess> sourceProcess =
                std::reinterpret_pointer_cast<SourceProcess>(source);
            InitializeProcessMonitor(sourceProcess);
            */
            break;
        }
        }
    }

    // Create and start EventMonitor if there are event channels
    if (!eventChannels.empty())
    {

        

        CreateEventMonitor(
            eventChannels,
            eventMonMultiLine,
            eventMonStartAtOldestRecord,
            eventCustomLogFormat);
    }

    // Create and start EtwMonitor if there are ETW providers
    //disable first
    /*
    if (!etwProviders.empty())
    {
        CreateEtwMonitor(
            etwProviders,
            etwCustomLogFormat);
    }
    */
}

/// <summary>
/// Initialize the Event Log Monitor
/// </summary>
/// <param name="sourceEventLog">The EventLog source settings</param>
/// <param name="eventChannels">The vector to store EventLog channels</param>
void MainService::InitializeEventLogMonitor(
    std::shared_ptr<SourceEventLog> sourceEventLog,
    std::vector<EventLogChannel>& eventChannels,
    bool& eventMonMultiLine,
    bool& eventMonStartAtOldestRecord,
    std::wstring& eventCustomLogFormat)
{
    for (auto channel : sourceEventLog->Channels)
    {
        eventChannels.push_back(channel);
    }

    eventMonMultiLine = sourceEventLog->EventFormatMultiLine;
    eventMonStartAtOldestRecord = sourceEventLog->StartAtOldestRecord;
    eventCustomLogFormat = sourceEventLog->CustomLogFormat;
}

/// <summary>
/// Instantiate the EventMonitor
/// </summary>
/// <param name="eventChannels">The list of EventLog channels</param>
void MainService::CreateEventMonitor(
    std::vector<EventLogChannel>& eventChannels,
    bool eventMonMultiLine,
    bool eventMonStartAtOldestRecord,
    const std::wstring& eventCustomLogFormat)
{
    try
    {

        g_eventMon = std::make_unique<EventMonitor>(
            eventChannels,
            eventMonMultiLine,
            eventMonStartAtOldestRecord,
            logFormat,
            m_eventQueueManager,
            eventCustomLogFormat
        );
        g_endpointMon = std::make_unique<EndpointMonitor>(
            m_eventQueueManager,
            eventCustomLogFormat
        );
    }
    catch (std::exception& ex)
    {
        LOG(Logger::ERRORS, Utility::FormatString(L"Instantiation of an EventMonitor object failed. %S", ex.what()).c_str()
        );
    }
    catch (...)
    {
        LOG(Logger::ERRORS, L"Instantiation of an EventMonitor object failed. Unknown error occurred.");
    }
}