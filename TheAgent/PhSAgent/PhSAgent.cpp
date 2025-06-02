// PhSAgent.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#pragma region Includes
#include <iostream>
#include <stdio.h>
#include <windows.h>
#include "ServiceInstaller.h"
#include "ServiceBase.h"
#include "MainService.h"
#include "Logger.h"
#include "pch.h"
#pragma endregion

#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "ws2_32.lib")  // For ntohs function
#pragma comment(lib, "shlwapi.lib")

// 
// Settings of the service
// 

// Internal name of the service
#define SERVICE_NAME             L"PhSAgent"

// Displayed name of the service
#define SERVICE_DISPLAY_NAME     L"PhSAgent"

// Service start options.
#define SERVICE_START_TYPE       SERVICE_DEMAND_START

// List of service dependencies - "dep1\0dep2\0\0"
#define SERVICE_DEPENDENCIES     L""

// The name of the account under which the service should run
//#define SERVICE_ACCOUNT          L"NT AUTHORITY\\LocalService"
#define SERVICE_ACCOUNT          NULL

// The password to the service account name
#define SERVICE_PASSWORD         NULL

// Initialize global variables to nullptr
HMODULE hModule = nullptr;   // Global DLL handle, initially null
LogFunction logFunc = nullptr; // Global function pointer, initially null

std::string wstr2str(const std::wstring& wstr) {
    // First, get the required size for the destination buffer
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);

    // Allocate a buffer to hold the converted string
    std::string str(size_needed, 0);

    // Perform the conversion to UTF-8
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);

    return str;
}

std::string getPath(const std::wstring& wstr) {
    // Allocate buffer to hold the full path (MAX_PATH is a common size)
    wchar_t path[MAX_PATH];

    // Get the full path of the executable (NULL for the current process)
    DWORD result = GetModuleFileNameW(NULL, path, MAX_PATH);

    if (result == 0) {
        std::wcerr << L"Error getting module file name" << std::endl;
        return "";
    }

    // Find the last backslash in the path to separate the directory
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash != NULL) {
        // Null-terminate the string at the last backslash to get the directory
        *lastSlash = L'\0';
    }

    // Append "log.txt" to the directory path
    std::wstring logFilePath = std::wstring(path) + wstr;

    std::string str = wstr2str(logFilePath);
    return str;
}

std::wstring getPathW(const std::wstring& wstr) {
    // Allocate buffer to hold the full path (MAX_PATH is a common size)
    wchar_t path[MAX_PATH];

    // Get the full path of the executable (NULL for the current process)
    DWORD result = GetModuleFileNameW(NULL, path, MAX_PATH);

    if (result == 0) {
        std::wcerr << L"Error getting module file name" << std::endl;
        return L"";
    }

    // Find the last backslash in the path to separate the directory
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash != NULL) {
        // Null-terminate the string at the last backslash to get the directory
        *lastSlash = L'\0';
    }

    // Append "log.txt" to the directory path
    std::wstring logFilePath = std::wstring(path) + wstr;

    return logFilePath;
}

//
//  FUNCTION: wmain(int, wchar_t *[])
//
//  PURPOSE: entrypoint for the application.
// 
//  PARAMETERS:
//    argc - number of command line arguments
//    argv - array of command line arguments
//
//  RETURN VALUE:
//    none
//
//  COMMENTS:
//    wmain() either performs the command line task, or run the service.
//
int wmain(int argc, wchar_t* argv[])
{
    Logger::getInstance().setLogFile(getPath(L"\\log.txt"));
    LOGW(Logger::INFO, L"Application called");
    //Logger::getInstance().printInstanceAddress();


    if ((argc > 1) && ((*argv[1] == L'-' || (*argv[1] == L'/'))))
    {
        if (_wcsicmp(L"install", argv[1] + 1) == 0)
        {
            // Install the service when the command is 
            // "-install" or "/install".
            InstallService(
                SERVICE_NAME,               // Name of service
                SERVICE_DISPLAY_NAME,       // Name to display
                SERVICE_START_TYPE,         // Service start type
                SERVICE_DEPENDENCIES,       // Dependencies
                SERVICE_ACCOUNT,            // Service running account
                SERVICE_PASSWORD            // Password of the account
            );
        }
        else if (_wcsicmp(L"remove", argv[1] + 1) == 0)
        {
            // Uninstall the service when the command is 
            // "-remove" or "/remove".
            UninstallService(SERVICE_NAME);
        }
        else if (_wcsicmp(L"service", argv[1] + 1) == 0)
        {
            // run the service when the command is 
            // "-service" or "/service".
            MainService service(SERVICE_NAME);
            if (!CServiceBase::Run(service))
            {
                wprintf(L"Service failed to run w/err 0x%08lx\n", GetLastError());
            }
        }
    }
    else
    {
        wprintf(L"Parameters:\n");
        wprintf(L" -install  to install the service.\n");
        wprintf(L" -remove   to remove the service.\n");

    }

    return 0;
}