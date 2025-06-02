//
// Copyright (c) .
// Licensed under the MIT license.
//

#include "pch.h"
#include "Logger.h"
#include "EventQueueManager.h"

using namespace std;


///
/// EndpointMonitor.cpp
///
/// Monitors endpoint's status and antivirus status and settings.
///
/// The destructor signals the stop event and waits up to MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS for the monitoring
/// thread to exit. To prevent the thread from out-living Monitor, the destructor fails fast
/// if the wait fails or times out. This also ensures the callback is not being called and will not be
/// called once Monitor is destroyed.
///


EndpointMonitor::EndpointMonitor(
    _In_ EventQueueManager& eventQueueManager,
    _In_ std::wstring CustomLogFormat = L""
    ) :
    m_eventQueueManager(eventQueueManager),
    m_customLogFormat(CustomLogFormat)
{
    m_stopEvent = NULL;
    m_eventMonitorThread = NULL;

    m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    if (!m_stopEvent)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateEvent");
    }

    m_eventMonitorThread = CreateThread(
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)&EndpointMonitor::StartEventMonitorStatic,
        this,
        0,
        nullptr
    );

    if (!m_eventMonitorThread)
    {
        throw std::system_error(std::error_code(GetLastError(), std::system_category()), "CreateThread");
    }
}

EndpointMonitor::~EndpointMonitor()
{
    if (!SetEvent(m_stopEvent))
    {
        LOG(Logger::ERRORS, Utility::FormatString(L"Failed to gracefully stop event log monitor %lu", GetLastError()).c_str()
        );
    }
    else
    {
        //
        // Wait for watch thread to exit.
        //
        DWORD waitResult = WaitForSingleObject(m_eventMonitorThread, EVENT_MONITOR_THREAD_EXIT_MAX_WAIT_MILLIS);

        if (waitResult != WAIT_OBJECT_0)
        {
            HRESULT hr = (waitResult == WAIT_FAILED) ? HRESULT_FROM_WIN32(GetLastError())
                                                           : HRESULT_FROM_WIN32(waitResult);
        }
    }

    if (!m_eventMonitorThread)
    {
        CloseHandle(m_eventMonitorThread);
    }

    if (!m_stopEvent)
    {
        CloseHandle(m_stopEvent);
    }
}

///
/// Entry for the spawned event monitor thread.
///
/// \param Context Callback context to the event monitor thread.
///                It's EventMonitor object that started this thread.
///
/// \return Status of event monitoring operation.
///
DWORD
EndpointMonitor::StartEventMonitorStatic(
    _In_ LPVOID Context
    )
{
    auto pThis = reinterpret_cast<EndpointMonitor*>(Context);
    try
    {
        DWORD status = pThis->StartEventMonitor();
        if (status != ERROR_SUCCESS)
        {
            LOG(Logger::ERRORS, Utility::FormatString(L"Failed to start event log monitor. Error: %lu", status).c_str()
            );
        }
        return status;
    }
    catch (std::exception& ex)
    {
        LOG(Logger::ERRORS, Utility::FormatString(L"Failed to start event log monitor. %S", ex.what()).c_str()
        );
        return E_FAIL;
    }
    catch (...)
    {
        LOG(Logger::ERRORS, Utility::FormatString(L"Failed to start event log monitor. Unknown error occurred.").c_str()
        );
        return E_FAIL;
    }
}


///
/// Entry for the spawned event monitor thread. Loops to wait for either the stop event in which case it exits
/// or for events to be arrived. When new events are arrived, it invokes the callback, resets,
/// and starts the wait again.
///
/// \return Status of event monitoring operation.
///
DWORD
EndpointMonitor::StartEventMonitor()
{
    DWORD status = ERROR_SUCCESS;

    if (status == ERROR_SUCCESS)
    {
        while (true)
        {
            __get();

            DWORD wait = WaitForSingleObject(m_stopEvent, 60000);

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
                    GetLastError()).c_str()
                    );
                break;
            }
        }
    }

    return status;
}


///
/// Enumerate the events in the result set.
///
/// \param EventChannels         The handle to the subscription that EvtSubscribe function returned.
///
/// \return A DWORD with a windows error value. If the function succeeded, it returns
///     ERROR_SUCCESS.
///
DWORD
EndpointMonitor::EnumerateResults(
    _In_ EVT_HANDLE hResults
    )
{
    DWORD status = ERROR_SUCCESS;
    EVT_HANDLE hEvents[EVENT_ARRAY_SIZE];
    DWORD dwReturned = 0;

    while (true)
    {
        //
        // Get a block of events from the result set.
        //
        if (!EvtNext(hResults, EVENT_ARRAY_SIZE, hEvents, INFINITE, 0, &dwReturned))
        {
            if (ERROR_NO_MORE_ITEMS != (status = GetLastError()))
            {
                LOG(Logger::ERRORS, Utility::FormatString(L"Failed to query next event. Error: %lu.", status).c_str()
                );
            }

            goto cleanup;
        }

        //
        // For each event, call the PrintEvent function which renders the
        // event for display.
        //
        for (DWORD i = 0; i < dwReturned; i++)
        {
            status = PrintEvent(hEvents[i]);

            if (ERROR_SUCCESS != status)
            {
                LOG(Logger::ERRORS, Utility::FormatString(
                        L"Failed to render event log event. The event will not be processed. Error: %lu.",
                        status
                    ).c_str()
                );
                status = ERROR_SUCCESS;
            }

            EvtClose(hEvents[i]);
            hEvents[i] = NULL;
        }
    }

cleanup:

    //
    // Closes any events in case an error occurred above.
    //
    for (DWORD i = 0; i < dwReturned; i++)
    {
        if (NULL != hEvents[i])
        {
            EvtClose(hEvents[i]);
        }
    }

    return status;
}

///
/// Constructs an EventLog object with the contents of an event's value paths.
///
/// \param EventHandle  Supplies a handle to an event, used to extract value paths.
/// \param AdditionalValuePaths  Supplies additional values to include in the returned EventLog object.
///
/// \return DWORD
///
DWORD
EndpointMonitor::PrintEvent(
    _In_ const HANDLE& EventHandle
    )
{
    DWORD status = ERROR_SUCCESS;
    DWORD bytesWritten = 0;
    EVT_HANDLE renderContext = NULL;
    EVT_HANDLE publisher = NULL;

    // struct to hold the Event log entry and later format print
    EventLogEntry logEntry;
    EventLogEntry* pLogEntry = &logEntry;

    static constexpr LPCWSTR defaultValuePaths[] = {
        L"Event/System/Provider/@Name",
        L"Event/System/Channel",
        L"Event/System/EventID",
        L"Event/System/Level",
        L"Event/System/TimeCreated/@SystemTime",
    };

    static const std::vector<std::wstring> c_LevelToString =
    {
        L"Unknown",
        L"Critical",
        L"Error",
        L"Warning",
        L"Information",
        L"Verbose",
    };

    static const DWORD defaultValuePathsCount = sizeof(defaultValuePaths) / sizeof(LPCWSTR);

    try
    {
        //
        // Construct the value paths that will be used to query the events
        //
        std::vector<LPCWSTR> valuePaths(defaultValuePaths, defaultValuePaths + defaultValuePathsCount);

        //
        // Collect event system properties
        //
        renderContext = EvtCreateRenderContext(
            static_cast<DWORD>(valuePaths.size()),
            &valuePaths[0],
            EvtRenderContextValues
        );

        if (!renderContext)
        {
            return GetLastError();
        }

        DWORD propertyCount = 0;
        DWORD bufferSize = 0;
        std::vector<EVT_VARIANT> variants;

        EvtRender(renderContext, EventHandle, EvtRenderEventValues, 0, nullptr, &bufferSize, &propertyCount);
        if (ERROR_INVALID_HANDLE == GetLastError())
        {
            status = ERROR_INVALID_HANDLE;
        }

        status = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER == status || ERROR_EVT_UNRESOLVED_VALUE_INSERT == status)
        {
            status = ERROR_SUCCESS;
        }

        if (status == ERROR_SUCCESS)
        {
            //
            // Allocate more memory to accommodate modulus
            //
            variants.resize((bufferSize / sizeof(EVT_VARIANT)) + 1, EVT_VARIANT{});
            if(!EvtRender(
                renderContext,
                EventHandle,
                EvtRenderEventValues,
                bufferSize,
                &variants[0],
                &bufferSize,
                &propertyCount))
            {
                status = GetLastError();

                LOG(Logger::ERRORS, Utility::FormatString(L"Failed to render event. Error: %lu", status).c_str()
                );
            }
        }

        if (status == ERROR_SUCCESS)
        {
            //
            // Extract the variant values for each queried property. If the variant failed to get a valid type
            // set a default value.
            //
            std::wstring providerName = (EvtVarTypeString != variants[EvtSystemProviderName].Type) ? L"" : variants[EvtSystemProviderName].StringVal;
            std::wstring channelName = (EvtVarTypeString != variants[1].Type) ? L"" : variants[1].StringVal;
            pLogEntry->eventId = (EvtVarTypeUInt16 != variants[2].Type) ? 0 : variants[2].UInt16Val;
            UINT8 level = (EvtVarTypeByte != variants[3].Type) ? 0 : variants[3].ByteVal;
            ULARGE_INTEGER fileTimeAsInt{};
            fileTimeAsInt.QuadPart = (EvtVarTypeFileTime != variants[4].Type) ? 0 : variants[4].FileTimeVal;
            FILETIME fileTimeCreated{
                fileTimeAsInt.LowPart,
                fileTimeAsInt.HighPart
            };

            //
            // Collect user message
            //
            publisher = EvtOpenPublisherMetadata(nullptr, providerName.c_str(), nullptr, 0, 0);

            if (publisher)
            {
                EvtFormatMessage(publisher, EventHandle, 0, 0, nullptr, EvtFormatMessageEvent, 0, nullptr, &bufferSize);
                status = GetLastError();

                if (status != ERROR_EVT_MESSAGE_NOT_FOUND)
                {
                    if (ERROR_INSUFFICIENT_BUFFER == status || ERROR_EVT_UNRESOLVED_VALUE_INSERT == status)
                    {
                        status = ERROR_SUCCESS;
                    }

                    if (m_eventMessageBuffer.capacity() < bufferSize)
                    {
                        m_eventMessageBuffer.resize(bufferSize);
                    }

                    if (!EvtFormatMessage(
                        publisher,
                        EventHandle,
                        0,
                        0,
                        nullptr,
                        EvtFormatMessageEvent,
                        bufferSize,
                        &m_eventMessageBuffer[0],
                        &bufferSize))
                    {
                        status = GetLastError();
                    }
                }
                else
                {
                    status = ERROR_SUCCESS;
                }
            }

            if (status == ERROR_SUCCESS)
            {
                pLogEntry->source = L"EventLog";
                pLogEntry->eventSource = providerName;
                pLogEntry->eventTime = Utility::FileTimeToString(fileTimeCreated);
                pLogEntry->eventChannel = channelName;
                pLogEntry->eventLevel = c_LevelToString[static_cast<UINT8>(level)];
                pLogEntry->eventMessage = (LPWSTR)(&m_eventMessageBuffer[0]);

                std::wstring formattedEvent;
                if (Utility::CompareWStrings(m_logFormat, L"Custom")) {
                    formattedEvent = Utility::FormatEventLineLog(m_customLogFormat, pLogEntry, pLogEntry->source);
                } else {
                    std::wstring logFmt;
                    if (Utility::CompareWStrings(m_logFormat, L"XML")) {
                        logFmt = L"<Log><Source>%s</Source><LogEntry><EventSource>%s</EventSource><Time>%s</Time>"
                                 L"<Channel>%s</Channel><Level>%s</Level>"
                                 L"<EventId>%u</EventId><Message>%s</Message>"
                                 L"</LogEntry></Log>";
                    } else {
                        logFmt = L"{\"Source\": \"%s\","
                                 L"\"LogEntry\": {"
                                 L"\"EventSource\": \"%s\","
                                 L"\"Time\": \"%s\","
                                 L"\"Channel\": \"%s\","
                                 L"\"Level\": \"%s\","
                                 L"\"EventId\": %u,"
                                 L"\"Message\": \"%s\""
                                 L"}}";

                        // sanitize message
                        std::wstring msg(m_eventMessageBuffer.begin(), m_eventMessageBuffer.end());
                        Utility::SanitizeJson(msg);
                        pLogEntry->eventMessage = msg;
                    }

                    formattedEvent = Utility::FormatString(
                        logFmt.c_str(),
                        pLogEntry->source.c_str(),
                        pLogEntry->eventSource.c_str(),
                        pLogEntry->eventTime.c_str(),
                        pLogEntry->eventChannel.c_str(),
                        pLogEntry->eventLevel.c_str(),
                        pLogEntry->eventId,
                        pLogEntry->eventMessage.c_str()
                    );
                }

                //LOGW(Logger::INFO, formattedEvent);
                //m_eventQueueManager.Push(L"EventLogs.log", formattedEvent);
                m_eventQueueManager.Push(formattedEvent);
                //std::wstring content = m_eventQueueManager.Peek();
                //m_eventQueueManager.Pop();
                //Logger::getInstance().printInstanceAddress();
                //LOG(Logger::INFO, content);
            }
        }
    }
    catch(...)
    {
        LOG(Logger::WARNING, L"Failed to render event log event. The event will not be processed.");
    }

    if (publisher)
    {
        EvtClose(publisher);
    }

    if (renderContext)
    {
        EvtClose(renderContext);
    }

    return status;
}


std::wstring EndpointMonitor::EventFieldsMapping(_In_ std::wstring eventField, _In_ void* pLogEntryData)
{
    std::wostringstream oss;
    EventLogEntry* pLogEntry = (EventLogEntry*)pLogEntryData;

    if (Utility::CompareWStrings(eventField, L"TimeStamp")) oss << pLogEntry->eventTime;
    if (Utility::CompareWStrings(eventField, L"Severity")) oss << pLogEntry->eventLevel;
    if (Utility::CompareWStrings(eventField, L"Source")) oss << pLogEntry->source;
    if (Utility::CompareWStrings(eventField, L"EventSource")) oss << pLogEntry->eventSource;
    if (Utility::CompareWStrings(eventField, L"EventID")) oss << pLogEntry->eventId;
    if (Utility::CompareWStrings(eventField, L"Message")) oss << pLogEntry->eventMessage;

    return oss.str();
}

//#include <iostream>
//#include <string>
#include <regex>
#include <cwchar>
#include <exception>
#include <iostream>
#include <string>
//#include <map>
//#include <sstream>
//#include <windows.h>

std::map<std::wstring, std::wstring> EndpointMonitor::parseSettings(const std::wstring& output) {
    std::map<std::wstring, std::wstring> resultMap;

    // Regex pattern to match key-value pairs: key: value
    std::wregex keyValuePattern(L"\\s*(\\S+)\\s*:\\s*(.*)\\s*");

    std::wsmatch match;
    std::wstring line;

    std::wstringstream ss(output);
    while (std::getline(ss, line)) {
        if (std::regex_search(line, match, keyValuePattern)) {
            // The first capture group is the key, and the second is the value
            std::wstring key = match[1].str();
            std::wstring value = match[2].str();

            // Insert the key-value pair into the map
            resultMap[key] = value;
        }
    }

    return resultMap;
}

std::wstring EndpointMonitor::toJson(const std::map<std::wstring, std::wstring>& settings) {
    std::wstring json = L"{\n";
    for (const auto& pair : settings) {
        json += L"  \"" + pair.first + L"\": \"" + pair.second + L"\",\n";
    }
    // Remove the last comma and newline
    if (!settings.empty()) {
        json = json.substr(0, json.size() - 2) + L"\n";
    }
    json += L"}";

    return json;
}



// Function to parse the file and extract settings and values using regular expressions
map<wstring, wstring> parse_settings(const wstring& filePath) {
    ifstream file(filePath);
    string line;
    map<wstring, wstring> settings;

    // Error handling: Check if the file could not be opened
    if (!file.is_open()) {
        wcerr << L"Error: Unable to open file " << filePath << endl;
        return settings;
    }

    // Regular expression to match lines like "SettingName = Value"
    //std::wregex keyValuePattern(L"\\s*(\\S+)\\s*:\\s*(.*)\\s*");
    regex settingRegex(R"(\s*(\S+)\s*:\s*(.*)\s*)");
    //regex settingRegex(R"((\S+)\s*=\s*(\S+))");

    while (getline(file, line)) {
        if (file.fail()) {  // Check for read failure
            wcerr << L"Error reading from file " << filePath << endl;
            break;  // Exit the loop if a read failure occurs
        }

        smatch match;
        if (regex_search(line, match, settingRegex)) {
            if (match.size() == 3) {
                wstring setting = Utility::string_to_wstring(match[1].str());
                wstring value = Utility::string_to_wstring(match[2].str());
                settings[setting] = value;
            }
        }
    }

    // Check if the file ended prematurely or if there were issues
    if (file.bad()) {
        wcerr << L"Error occurred while reading the file " << filePath << endl;
    }

    file.close();  // Close the file
    return settings;
}

#include <iostream>
#include <fstream>
#include <string>

using namespace std;

// Function to read the file and return its content as a wstring
wstring read_file_to_wstring(const wstring& filePath) {
    wifstream file(filePath);
    wstring content;

    // Error handling: Check if the file could not be opened
    if (!file.is_open()) {
        wcerr << L"Error: Unable to open file " << filePath << endl;
        return content;
    }

    // Read each line from the file and append it to content
    wstring line;
    while (getline(file, line)) {
        if (file.fail()) {  // Check for read failure
            wcerr << L"Error reading from file " << filePath << endl;
            break;  // Exit the loop if a read failure occurs
        }
        // Convert the line to wstring and append it to the content
        //content += Utility::string_to_wstring(line) + L"\n";
        content += line + L"\n";
    }

    // Check if the file ended prematurely or if there were issues
    if (file.bad()) {
        wcerr << L"Error occurred while reading the file " << filePath << endl;
    }

    file.close();  // Close the file
    return content;
}


// Function to convert the settings map to a JSON-like wstring
wstring to_json_like(const map<wstring, wstring>& settings) {
    wstringstream jsonStream;
    jsonStream << L"{\n";
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        jsonStream << L"  \"" << it->first << L"\": \"" << it->second << L"\"";
        if (next(it) != settings.end()) {
            jsonStream << L",";
        }
        jsonStream << L"\n";
    }
    jsonStream << L"}";
    return jsonStream.str();
}

// Helper function to get the directory where the executable is located
wstring get_executable_directory() {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        PathRemoveFileSpecW(exePath);  // Remove the executable name
        return wstring(exePath) + L"\\";
    }
    return L"";
}

int main() {
    // Step 1: Get the executable's directory
    wstring executableDir = get_executable_directory();
    wstring outputFile = executableDir + L"output.txt";  // Output file in the same folder as the executable

    // PowerShell command to execute (modify as needed)
    wstring powershellCommand = L"Get-ItemProperty -Path 'HKCU:\\Software\\SomeKey'";

    // Step 2: Execute PowerShell command and redirect output to file
    DWORD timeoutMs = 5000;  // 5 seconds timeout
    if (!Utility::executePowerShellCommand(powershellCommand, outputFile, timeoutMs)) {
        wcerr << L"Error executing PowerShell command." << endl;
        return 1;
    }

    // Step 3: Parse the output file and extract settings and values
    map<wstring, wstring> settings = parse_settings(outputFile);

    // Step 4: Convert the settings to a JSON-like wstring
    wstring jsonResult = to_json_like(settings);

    // Step 5: Output the result
    wcout << L"JSON-like Output:\n" << jsonResult << endl;

    return 0;
}

int EndpointMonitor::__get() {
    try {
        // Map of PowerShell commands to execute
        std::map<std::wstring, std::wstring> mapCommands = {
            {L"Set-ExecutionPolicy -ExecutionPolicy Unrestricted -Force", L"setpolicy.log"},
            {L"Import-Module Defender; Get-MpComputerStatus", L"windefender01.log"},
            {L"get-mppreference", L"windefender02.log"},
            {L"Get-WmiObject Win32_ComputerSystem", L"hostinfo.log"},
            {L"Get-WmiObject Win32_OperatingSystem", L"sysinfo.log"},
            {L"Get-NetIPAddress", L"ipadd.log"}
        };

        /////// Print the map contents
        for (const auto& pair : mapCommands) {
            std::wcout << pair.first << L" => " << pair.second << std::endl;
        }

        // Step 1: Get the executable's directory
        wstring executableDir = get_executable_directory();

        DWORD timeoutMs = 5000;  // 5 seconds timeout
        std::map<std::wstring, std::wstring> allSettings;

        for (const auto& pair : mapCommands) {
            wstring outputFile = executableDir + pair.second;  // Output file in the same folder as the executable
            
            // Execute each PowerShell command
            LOG(Logger::INFO, L"execute: " + pair.first);
            Utility::executePowerShellCommand(pair.first, outputFile, timeoutMs);

            // Step 3: Parse the output file and extract settings and values
            //wstring settings = L"";
            wstring settings = read_file_to_wstring(outputFile);

            //m_eventQueueManager.Push(outputFile, settings);
            //m_eventQueueManager.Push(settings);
        }

        // Convert aggregated results to JSON-like wstring
        //std::wstring jsonOutput = toJson(allSettings);

        // Display the final JSON
        //std::wcout << jsonOutput << std::endl;
        //LOGW(Logger::INFO, jsonOutput);
        //m_eventQueueManager.Push(jsonOutput);
        //m_eventQueueManager.Push(output);
        //std::wstring content = m_eventQueueManager.Peek();
        //m_eventQueueManager.Pop();
        //Logger::getInstance().printInstanceAddress();
        //LOG(Logger::INFO, content);

    }
    catch (const std::exception& e) {
        // Calculate the required buffer size for the wide string
        size_t len = std::strlen(e.what()) + 1;  // +1 for the null terminator
        std::wstring werror(len, L'\0');

        // Use mbstowcs_s to safely convert the multibyte string to a wide string
        size_t convertedChars = 0;
        errno_t err = mbstowcs_s(&convertedChars, &werror[0], len, e.what(), len);

        if (err != 0) {
            // Handle the error if conversion fails (e.g., incorrect input or buffer overflow)
            std::wcerr << L"Error: Conversion failed." << std::endl;
            return 0;  // or handle the error appropriately
        }

        // Now you can use werror as a wide string
        std::wcerr << L"Error: " << werror << std::endl;

        // Assuming LOGW accepts std::wstring, you can pass werror directly
        LOGW(Logger::ERRORS, Utility::FormatString(L"Failed . %s", werror.c_str()));
    }

    return 0;
}