//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#include "pch.h"
#include "Logger.h"
#include <regex>

using namespace std;


///
/// Utility.cpp
///
/// Contains utility functions to format strings, convert time to string, etc.
///


///
/// Returns the string representation of a SYSTEMTIME structure that can be used
/// in an XML query for Windows Event collection.
///
/// \param SystemTime         SYSTEMTIME with the time to format.
///
/// \return The string with the human-readable time.
///
std::wstring
Utility::SystemTimeToString(
    SYSTEMTIME SystemTime
    )
{
    constexpr size_t STR_LEN = 64;
    wchar_t dateStr[STR_LEN] = { 0 };

#pragma warning(suppress : 38042)
    GetDateFormatEx(0, 0, &SystemTime, L"yyyy-MM-dd", dateStr, STR_LEN, 0);

    wchar_t timeStr[STR_LEN] = { 0 };
    GetTimeFormatEx(0, 0, &SystemTime, L"HH:mm:ss", timeStr, STR_LEN);

    return FormatString(L"%sT%s.000Z", dateStr, timeStr);
}


///
/// Returns a string representation of a FILETIME.
///
/// \param FileTime         FILETIME with the time to format.
///
/// \return The string with the human-readable time.
///
std::wstring
Utility::FileTimeToString(
    FILETIME FileTime
    )
{
    SYSTEMTIME systemTime;
    FileTimeToSystemTime(&FileTime, &systemTime);
    return SystemTimeToString(systemTime);
}

///
/// Returns a formatted wide string.
///
/// \param FormatString         Format string that follows the same specifications as format in printf.
/// \param ...                  Variable arguments list
///
/// \return The composed string.
///
std::wstring
Utility::FormatString(
    _In_ _Printf_format_string_ LPCWSTR FormatString,
    ...
    )
{
    std::wstring result = L"";
    va_list vaList;
    va_start(vaList, FormatString);

    size_t length = ::_vscwprintf(FormatString, vaList);

    try
    {
        result.resize(length);

        if (-1 == ::vswprintf_s(&result[0], length + 1, FormatString, vaList))
        {
            result.clear();
        }
    }
    catch (...) {
        result.clear();
    }

    va_end(vaList);

    return result;
}

///
/// Verify if the input stream is in UTF-8 format.
/// UTF-8 is the encoding of Unicode based on Internet Society RFC2279
/// ( See https://tools.ietf.org/html/rfc2279 )
///
/// \param lpstrInputStream     Pointer to a buffer with the wide text to evaluate.
/// \param iLen                 Size of the buffer.
///
/// \return If the text is UTF-8, return true. Otherwise, false.
///
bool
Utility::IsTextUTF8(
    LPCSTR InputStream,
    int Length
    )
{
    int nChars = MultiByteToWideChar(CP_UTF8,
                                      MB_ERR_INVALID_CHARS,
                                      InputStream,
                                      Length,
                                      NULL,
                                      0);

    return nChars > 0 || GetLastError() != ERROR_NO_UNICODE_TRANSLATION;
}

///
/// Verify if the input stream is in Unicode format.
///
/// \param lpstrInputStream     Pointer to a buffer with the wide text to evaluate.
/// \param iLen                 Size of the buffer.
///
/// \return If the text is unicode, return true. Otherwise, false.
///
bool
Utility::IsInputTextUnicode(
    LPCSTR InputStream,
    int Length
    )
{
    int  iResult = ~0; // turn on IS_TEXT_UNICODE_DBCS_LEADBYTE
    bool bUnicode;

    bUnicode = IsTextUnicode(InputStream, Length, &iResult);

    //
    // If the only hint is based on statistics, assume ANSI for short strings
    // This protects against short ansi strings like "this program can break" from being
    // detected as unicode
    //
    if (bUnicode && iResult == IS_TEXT_UNICODE_STATISTICS && Length < 100)
    {
        bUnicode = false;
    }

    return bUnicode;
}


///
/// Get the short path name of the file. If the function
/// failed to get the short path, then it returns original path.
///
/// \param Path Full path of a file or directory
///
/// \return Short path of a file or directory
///
std::wstring
Utility::GetShortPath(
    _In_ const std::wstring& Path
    )
{
    DWORD bufSz = 1024;
    std::vector<wchar_t> buf(bufSz);

    DWORD pathSz = GetShortPathNameW(Path.c_str(), &(buf[0]), bufSz);
    if (pathSz >= bufSz)
    {
        buf.resize(pathSz + 1);
        if (GetShortPathNameW(Path.c_str(), &(buf[0]), pathSz + 1))
        {
            wstring shortPath = &buf[0];
            return shortPath.c_str();
        }
    }
    else if (pathSz)
    {
        wstring shortPath = &buf[0];
        return shortPath.c_str();
    }

    return Path;
}


///
/// Get the long path name of the file. If the function
/// failed to get the long path, then it returns original path.
///
/// \param Path Full path of a file or directory
///
/// \return Long path of a file or directory
///
std::wstring
Utility::GetLongPath(
    _In_ const std::wstring& Path
    )
{
    DWORD bufSz = 1024;
    std::vector<wchar_t> buf(bufSz);

    DWORD pathSz = GetLongPathNameW(Path.c_str(), &(buf[0]), bufSz);
    if(pathSz >= bufSz)
    {
        buf.resize(pathSz + 1);
        if (GetLongPathNameW(Path.c_str(), &(buf[0]), pathSz + 1))
        {
            wstring longPath = &buf[0];
            return longPath.c_str();
        }
    }
    else if (pathSz)
    {
        wstring longPath = &buf[0];
        return longPath.c_str();
    }

    return Path;
}

///
/// Replaces all the occurrences in a wstring.
///
/// \param Str      The string to search substrings and replace them.
/// \param From     The substring to being replaced.
/// \param To       The substring to replace.
///
/// \return A wstring.
///
std::wstring Utility::ReplaceAll(_In_ std::wstring Str, _In_ const std::wstring& From, _In_ const std::wstring& To) {
    size_t start_pos = 0;

    while ((start_pos = Str.find(From, start_pos)) != std::string::npos) {
        Str.replace(start_pos, From.length(), To);
        start_pos += To.length(); // Handles case where 'To' is a substring of 'From'
    }
    return Str;
}


/// 
/// helper function for a basic check if a string is a Number (JSON)
/// as per the JSON spec - https://www.json.org/json-en.html
/// only numbers not covered are those in scientific e-notation
/// 
bool Utility::isJsonNumber(_In_ std::wstring& str)
{
    wregex isNumber(L"(^\\-?\\d+$)|(^\\-?\\d+\\.\\d+)$");
    return regex_search(str, isNumber);
}

///
/// helper function to "sanitize" a string to be valid JSON
/// i.e. escape ", \r, \n and \ within a string
/// to \", \\r, \\n and \\ respectively
///
bool IsEscaped(const std::wstring& str, size_t pos) {
    int backslashCount = 0;
    for (int i = static_cast<int>(pos) - 1; i >= 0 && str[i] == L'\\'; --i) {
        backslashCount++;
    }
    return (backslashCount % 2) == 1; // Escaped if odd number of backslashes
}

void Utility::SanitizeJson(_Inout_ std::wstring& str)
{
    size_t i = 0;
    while (i < str.size()) {
        auto sub = str.substr(i, 1);
        if (sub == L"\"") {
            if (!IsEscaped(str, i)) {
                str.replace(i, 1, L"\\\"");
                i++;
            }
        }
        else if (sub == L"\\") {
            if (i == str.size() - 1 || str[i + 1] != L'\\') {
                str.replace(i, 1, L"\\\\");
                i++;
            }
            else {
                i += 2;
                continue;
            }
        }
        else if (sub == L"\n") {
            if (!IsEscaped(str, i)) {
                str.replace(i, 1, L"\\n");
                i++;
            }
        }
        else if (sub == L"\r") {
            if (!IsEscaped(str, i)) {
                str.replace(i, 1, L"\\r");
                i++;
            }
        }
        else if (sub == L"\t") {
            if (!IsEscaped(str, i)) {
                str.replace(i, 1, L"\\t");
                i++;
            }
        }
        i++;
    }
}


bool Utility::ConfigAttributeExists(AttributesMap& Attributes, std::wstring attributeName)
{
    auto it = Attributes.find(attributeName);
    return it != Attributes.end() && it->second != nullptr;
}

///
// Converts the time to wait to a large integer
///
LARGE_INTEGER Utility::ConvertWaitIntervalToLargeInt(_In_ int timeInterval)
{
    LARGE_INTEGER liDueTime{};

    int millisecondsToWait = timeInterval * 1000;
    liDueTime.QuadPart = -millisecondsToWait * 10000LL;  // wait time in 100 nanoseconds
    return liDueTime;
}

///
/// Returns the time (in seconds) to wait based on the specified waitInSeconds
///
int Utility::GetWaitInterval(_In_ std::double_t waitInSeconds, _In_ int elapsedTime)
{
    if (isinf(waitInSeconds))
    {
        return static_cast<int>(WAIT_INTERVAL);
    }

    if (waitInSeconds < WAIT_INTERVAL)
    {
        return static_cast<int>(waitInSeconds);
    }

    const auto remainingTime = static_cast<int>(waitInSeconds - elapsedTime);
    return remainingTime <= WAIT_INTERVAL ? remainingTime : WAIT_INTERVAL;
}

/// <summary>
/// Comparing wstrings with ignoring the case
/// </summary>
/// <param name="stringA"></param>
/// <param name="stringB"></param>
/// <returns></returns>
///
bool Utility::CompareWStrings(wstring stringA, wstring stringB)
{
    return stringA.size() == stringB.size() &&
        equal(
            stringA.cbegin(),
            stringA.cend(),
            stringB.cbegin(),
            [](wstring::value_type l1, wstring::value_type r1) {
                return towupper(l1) == towupper(r1);
            }
        );
}

std::wstring Utility::FormatEventLineLog(
    _In_ std::wstring customLogFormat,
    _In_ void* pLogEntry,
    _In_ std::wstring sourceType
)
{
    bool customJsonFormat = IsCustomJsonFormat(customLogFormat);

    size_t i = 0, j = 1;
    while (i < customLogFormat.size()) {
        auto sub = customLogFormat.substr(i, j - i);
        auto sub_length = sub.size();

        bool startsWithPercent = sub[0] == '%';
        bool endsWithPercent = sub[sub_length - 1] == '%';

        if (!startsWithPercent && !endsWithPercent) {
            j++, i++;
        } else if (startsWithPercent && endsWithPercent && sub_length > 1) {
            // Valid field name found in custom log format
            wstring fieldValue;
            auto fieldName = sub.substr(1, sub_length - 2);
            if (sourceType == L"ETW") {
                //fieldValue = EtwMonitor::EtwFieldsMapping(fieldName, pLogEntry);
            } else if (sourceType == L"EventLog") {
                fieldValue = EventMonitor::EventFieldsMapping(fieldName, pLogEntry);
            } else if (sourceType == L"File") {
                //fieldValue = LogFileMonitor::FileFieldsMapping(fieldName, pLogEntry);
            } else if (sourceType == L"Process") {
                //fieldValue = ProcessMonitor::ProcessFieldsMapping(fieldName, pLogEntry);
            }
            // Substitute the field name with value
            customLogFormat.replace(i, sub_length, fieldValue);

            i += fieldValue.length();
            j = i + 1;
        } else {
            j++;
        }
    }

    if(customJsonFormat)
        SanitizeJson(customLogFormat);

    return customLogFormat;
}

/// <summary>
/// check if custom format specified in config is JSON for sanitization purposes
/// </summary>
/// <param name="customLogFormat"></param>
/// <returns></returns>
bool Utility::IsCustomJsonFormat(_Inout_ std::wstring& customLogFormat)
{
    bool isCustomJSONFormat = false;

    auto npos = customLogFormat.find_last_of(L"|");
    std::wstring substr;
    if (npos != std::string::npos) {
        substr = customLogFormat.substr(npos + 1);
        substr.erase(std::remove(substr.begin(), substr.end(), ' '), substr.end());

        if (!substr.empty() && CompareWStrings(substr, L"JSON")) {
            customLogFormat = ReplaceAll(customLogFormat, L"'", L"~\"");
            isCustomJSONFormat = true;
        }

        customLogFormat = customLogFormat.substr(0, customLogFormat.find_last_of(L"|"));
    }
    return isCustomJSONFormat;
}

#include <string>
/// <summary>
/// execute PowerShell command
/// </summary>
/// <param name="command"></param>
/// <returns></returns>
#include <Windows.h>
#include <string>
#include <stdexcept>
#include <vector>
#include <codecvt>
#include <locale>

// Convert a UTF-8 encoded string to UTF-16 (std::wstring)
std::wstring ConvertUTF8ToUTF16(const std::string& utf8String) {
    // Create a UTF-8 to UTF-16 converter
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(utf8String);  // Converts UTF-8 string to UTF-16 string (std::wstring)
}

std::wstring Utility::executePowerShellCommand(const std::wstring& command, DWORD timeout) {
    HANDLE hPipeRead = NULL, hPipeWrite = NULL;
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE; // Allow child process to inherit the pipe handles
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe to capture the output
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0)) {
        throw std::runtime_error("Failed to create pipe.");
    }

    // Set up the process startup information
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(STARTUPINFOW));
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hPipeWrite;
    si.hStdError = hPipeWrite;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    std::wstring psCommand = L"powershell.exe -Command " + command;

    // Create the PowerShell process. The output of PS will be UTF8 when useing CreateProcess or output to pipe
    if (!CreateProcessW(NULL, &psCommand[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hPipeRead);
        CloseHandle(hPipeWrite);
        throw std::runtime_error("Failed to create process.");
    }

    // Wait for the process to complete or timeout
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeout);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(hPipeRead);
        CloseHandle(hPipeWrite);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        throw std::runtime_error("Process timed out and was terminated.");
    }

    // Close the write end of the pipe in the parent process
    CloseHandle(hPipeWrite);

    // Read the output from the pipe in UTF-8
    std::string resultUTF8;
    char buffer[40960]; // Buffer to read the output as UTF-8
    DWORD bytesRead;
    while (ReadFile(hPipeRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        resultUTF8.append(buffer, bytesRead);
    }

    // Close the read end of the pipe
    CloseHandle(hPipeRead);

    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Convert the UTF-8 result to UTF-16 (std::wstring)
    std::wstring resultUTF16 = ConvertUTF8ToUTF16(resultUTF8);

    return resultUTF16;
}


// Function to execute a PowerShell command and redirect its output to a file using CreateProcessW
bool Utility::executePowerShellCommand(const wstring& command, const wstring& outputFile, DWORD timeoutMs) {
    STARTUPINFOW si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES) };

    // Set up STARTUPINFO structure (no need for redirection handles since PowerShell will handle it)
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = 0; // No redirection needed here

    // Create the PowerShell command with redirection to a file
    wstring fullCommand = L"powershell.exe " + command + L" > " + outputFile;

    // Create the PowerShell process
    if (!CreateProcessW(NULL, const_cast<wchar_t*>(fullCommand.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        wcerr << L"Error creating PowerShell process!" << endl;
        LOG(Logger::ERRORS, L"Error creating PowerShell process!");
        return false;
    }

    // Wait for the PowerShell process to finish or timeout
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        // If the process timed out, terminate it
        TerminateProcess(pi.hProcess, 1);
        wcerr << L"PowerShell command timed out. Process terminated." << endl;
        LOG(Logger::ERRORS, L"PowerShell command timed out. Process terminated.");
    }

    // Close handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

// Helper function to convert a string to wstring
std::wstring Utility::string_to_wstring(const std::string& str) {
    wstring_convert<codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
}


std::string Utility::wstring_to_string(const std::wstring& wstr) {
    // First, get the required size for the destination buffer
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);

    // Allocate a buffer to hold the converted string
    std::string str(size_needed, 0);

    // Perform the conversion to UTF-8
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);

    return str;
}

/*
///////
///////
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// Read file into buffer
std::vector<unsigned char> Utility::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), {});
}

// Check for BOMs
std::string checkBOM(const std::vector<unsigned char>& buffer) {
    if (buffer.size() >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
        return "UTF-8 with BOM";
    if (buffer.size() >= 2 && buffer[0] == 0xFF && buffer[1] == 0xFE)
        return "UTF-16 LE with BOM";
    if (buffer.size() >= 2 && buffer[0] == 0xFE && buffer[1] == 0xFF)
        return "UTF-16 BE with BOM";
    return "";
}

// Check if valid UTF-8
bool isValidUTF8(const std::vector<unsigned char>& buffer) {
    size_t i = 0;
    while (i < buffer.size()) {
        unsigned char c = buffer[i];
        size_t bytes = 0;
        if ((c & 0x80) == 0x00) bytes = 1;            // ASCII
        else if ((c & 0xE0) == 0xC0) bytes = 2;
        else if ((c & 0xF0) == 0xE0) bytes = 3;
        else if ((c & 0xF8) == 0xF0) bytes = 4;
        else return false;

        if (i + bytes > buffer.size()) return false;
        for (size_t j = 1; j < bytes; ++j) {
            if ((buffer[i + j] & 0xC0) != 0x80) return false;
        }
        i += bytes;
    }
    return true;
}

// Simple UTF-16LE/BE guesser based on null-byte positions
std::string guessUTF16(const std::vector<unsigned char>& buffer) {
    size_t le_nulls = 0, be_nulls = 0;
    size_t samples = std::min<size_t>(buffer.size() / 2, 1000);

    for (size_t i = 0; i < samples * 2; i += 2) {
        if (buffer[i] == 0x00) ++be_nulls;
        if (buffer[i + 1] == 0x00) ++le_nulls;
    }

    if (le_nulls > samples * 0.5) return "UTF-16LE (guessed)";
    if (be_nulls > samples * 0.5) return "UTF-16BE (guessed)";
    return "";
}

std::string detectEncoding(const std::string& filePath) {
    auto buffer = readFile(filePath);
    if (buffer.empty()) return "File read error or empty";

    std::string bom = checkBOM(buffer);
    if (!bom.empty()) return bom;

    if (isValidUTF8(buffer)) return "UTF-8 (no BOM)";

    std::string utf16Guess = guessUTF16(buffer);
    if (!utf16Guess.empty()) return utf16Guess;

    return "ANSI / Unknown";
}

int main() {
    std::string filename = "test.txt";
    std::string encoding = detectEncoding(filename);
    std::cout << "Detected encoding: " << encoding << std::endl;
    return 0;
}
*/
