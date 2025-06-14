//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
#include "pch.h"
#include "ConfigFileParser.h"
#include "FileMonitorUtilities.h"

/// ConfigFileParser.cpp
///
/// Reads the configuration file content (as a string), parsing it with a
/// JsonFileParser object previously created.
///
/// The main entry point in this file is OpenConfigFile.
///

///
/// Open the config file and convert the document content into json
///
/// \param FileName       Config File name.
///
/// \return True if the configuration file was valid. Otherwise false
///
bool OpenConfigFile(_In_ const PWCHAR ConfigFileName, _Out_ LoggerSettings& Config)
{
    bool success;
    std::wifstream configFileStream(ConfigFileName);
    configFileStream.imbue(std::locale(configFileStream.getloc(),
        new std::codecvt_utf8_utf16<wchar_t, 0x10ffff, std::little_endian>));

    if (configFileStream.is_open())
    {
        try
        {
            //
            // Convert the document content to a string, to pass it to JsonFileParser constructor.
            //
            std::wstring configFileStr((std::istreambuf_iterator<wchar_t>(configFileStream)),
                std::istreambuf_iterator<wchar_t>());
            configFileStr.erase(remove(configFileStr.begin(), configFileStr.end(), 0xFEFF), configFileStr.end());

            JsonFileParser jsonParser(configFileStr);

            success = ReadConfigFile(jsonParser, Config);
        }
        catch (std::exception& ex)
        {
            LOG(Logger::ERRORS, Utility::FormatString(L"Failed to read json configuration file. %S", ex.what()).c_str());
            success = false;
        }
        catch (...)
        {
            LOG(Logger::ERRORS, Utility::FormatString(L"Failed to read json configuration file. Unknown error occurred.").c_str());
            success = false;
        }
    } else {
        LOG(Logger::ERRORS, Utility::FormatString(
                L"Configuration file '%s' not found. Logs will not be monitored.",
                ConfigFileName
            ).c_str()
        );
        success = false;
    }

    return success;
}

///
/// Read the root JSON of the config file
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Config       Returns a LoggerSettings struct with the values specified
///     in the config file.
///
/// \return True if the configuration file was valid. Otherwise false
///
bool
ReadConfigFile(
    _In_ JsonFileParser& Parser,
    _Out_ LoggerSettings& Config
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        LOG(Logger::ERRORS, L"Failed to parse configuration file. Object expected at the file's root");
        return false;
    }

    bool containsLogConfigTag = false;

    if (Parser.BeginParseObject())
    {
        do
        {
            const std::wstring key(Parser.GetKey());

            if (_wcsnicmp(key.c_str(), JSON_TAG_LOG_CONFIG, _countof(JSON_TAG_LOG_CONFIG)) == 0)
            {
                containsLogConfigTag = ReadLogConfigObject(Parser, Config);
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_LOGSQUEUESENDER, _countof(JSON_TAG_LOGSQUEUESENDER)) == 0)
            {
                ReadLogConfigObject(Parser, Config);
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_LOGSQUEUE, _countof(JSON_TAG_LOGSQUEUE)) == 0)
            {
                ReadLogConfigObject(Parser, Config);
            }
            else
            {
                Parser.SkipValue();
            }
        } while (Parser.ParseNextObjectElement());
    }

    return containsLogConfigTag;
}

///
/// Read LogConfig tag, that contains the config of the sources
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Config       Returns a LoggerSettings struct with the values specified
///                     in the config file.
///
/// \return True if LogConfig tag was valid. Otherwise false
///
bool
ReadLogConfigObject(
    _In_ JsonFileParser& Parser,
    _Out_ LoggerSettings& Config
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        LOG(Logger::ERRORS, L"Failed to parse configuration file. 'LogConfig' is expected to be an object");
        Parser.SkipValue();
        return false;
    }

    bool sourcesTagFound = false;
    if (Parser.BeginParseObject())
    {
        std::wstring key;
        do
        {
            key = Parser.GetKey();

            if (_wcsnicmp(key.c_str(), JSON_TAG_SOURCES, _countof(JSON_TAG_SOURCES)) == 0)
            {
                if (Parser.GetNextDataType() != JsonFileParser::DataType::Array)
                {
                    LOG(Logger::ERRORS, L"Failed to parse configuration file. 'sources' attribute expected to be an array");
                    Parser.SkipValue();
                    continue;
                }

                sourcesTagFound = true;

                if (!Parser.BeginParseArray())
                {
                    continue;
                }

                do
                {
                    AttributesMap sourceAttributes;

                    //
                    // Read all attributes of a source from the config file,
                    // instantiate it and add it to the end of the vector
                    //
                    if (ReadSourceAttributes(Parser, sourceAttributes)) {
                        if (!AddNewSource(Parser, sourceAttributes, Config.Sources))
                        {
                            LOG(Logger::ERRORS, L"Failed to parse configuration file. Error reading invalid source.");
                        }
                    }
                    else
                    {
                        LOG(Logger::ERRORS, L"Failed to parse configuration file. Error retrieving source attributes. Invalid source");
                    }

                    for (auto attributePair : sourceAttributes)
                    {
                        if (attributePair.second != nullptr)
                        {
                            delete attributePair.second;
                        }
                    }
                } while (Parser.ParseNextArrayElement());
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_LOG_FORMAT, _countof(JSON_TAG_LOG_FORMAT)) == 0)
            {
                Config.LogFormat = std::wstring(Parser.ParseStringValue());
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_SENDINTERVAL, _countof(JSON_TAG_SENDINTERVAL)) == 0)
            {
                Config.SendInterval = std::double_t(Parser.ParseNumericValue());

            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_SENDLMAXEVENT, _countof(JSON_TAG_SENDLMAXEVENT)) == 0)
            {
                Config.Max_send_events = std::double_t(Parser.ParseNumericValue());

            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_SENDFOLDERPATH, _countof(JSON_TAG_SENDFOLDERPATH)) == 0)
            {
                Config.SendLocalPath = std::wstring(Parser.ParseStringValue());
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_SENDURLPATH, _countof(JSON_TAG_SENDURLPATH)) == 0)
            {
                Config.SendURLPath = std::wstring(Parser.ParseStringValue());
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_QUEUEMAXEVENT, _countof(JSON_TAG_QUEUEMAXEVENT)) == 0)
            {
                Config.Max_queue_events = std::double_t(Parser.ParseNumericValue());
            }
            else
            {
                LOG(Logger::WARNING, Utility::FormatString(L"Error parsing configuration file. 'Unknow key %ws in the configuration file.", key.c_str()).c_str());
                Parser.SkipValue();
            }
        } while (Parser.ParseNextObjectElement());
    }

    return sourcesTagFound;
}

/*
///
/// Originally FileMonitorUtilities::ParseDirectoryValue(_Inout_ std::wstring& directory)
/// move here so not to copy the file FileMonitorUtilities.cpp
///
void ParseDirectoryValue(_Inout_ std::wstring& directory)
{
    // Replace all occurrences of forward slashes (/) with backslashes (\).
    std::replace(directory.begin(), directory.end(), L'/', L'\\');

    // Remove trailing backslashes
    while (!directory.empty() && directory[directory.size() - 1] == L'\\')
    {
        directory.resize(directory.size() - 1);
    }
}
*/

///
/// Look for all the attributes that a single 'source' object contains
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Config       Returns an AttributesMap, with all allowed tag names and theirs values
///
/// \return True if the attributes contained valid values. Otherwise false
///
bool
ReadSourceAttributes(
    _In_ JsonFileParser& Parser,
    _Out_ AttributesMap& Attributes
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        LOG(Logger::ERRORS, L"Failed to parse configuration file. Source item expected to be an object");
        Parser.SkipValue();
        return false;
    }

    bool success = true;

    if (Parser.BeginParseObject())
    {
        do
        {
            //
            // If source reading already fail, just skip attributes^M
            //
            if (!success)
            {
                Parser.SkipValue();
                continue;
            }

            const std::wstring key(Parser.GetKey());

            if (_wcsnicmp(key.c_str(), JSON_TAG_TYPE, _countof(JSON_TAG_TYPE)) == 0)
            {
                const auto& typeString = Parser.ParseStringValue();
                LogSourceType* type = nullptr;

                //
                // Check if the string is the name of a valid LogSourceType
                //
                int sourceTypeArraySize = sizeof(LogSourceTypeNames) / sizeof(LogSourceTypeNames[0]);
                for (int i = 0; i < sourceTypeArraySize; i++)
                {
                    if (_wcsnicmp(typeString.c_str(), LogSourceTypeNames[i], typeString.length()) == 0)
                    {
                        type = new LogSourceType;
                        *type = static_cast<LogSourceType>(i);
                    }
                }

                //
                // If the value isn't a valid type, fail.
                //
                if (type == nullptr)
                {
                    LOG(Logger::ERRORS, Utility::FormatString(
                            L"Error parsing configuration file. '%s' isn't a valid source type", typeString.c_str()
                        ).c_str()
                    );

                    success = false;
                }
                else
                {
                    Attributes[key] = type;
                }
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_CHANNELS, _countof(JSON_TAG_CHANNELS)) == 0)
            {
                if (Parser.GetNextDataType() != JsonFileParser::DataType::Array)
                {
                    LOG(Logger::ERRORS, L"Error parsing configuration file. 'channels' attribute expected to be an array");
                    Parser.SkipValue();
                    continue;
                }

                if (Parser.BeginParseArray())
                {
                    std::vector<EventLogChannel>* channels = new std::vector<EventLogChannel>();

                    //
                    // Get only the valid channels of this JSON object.
                    //
                    do
                    {
                        channels->emplace_back();
                        if (!ReadLogChannel(Parser, channels->back()))
                        {
                            LOG(Logger::ERRORS, L"Error parsing configuration file. Discarded invalid channel (it must have a non-empty 'name').");
                            channels->pop_back();
                        }
                    } while (Parser.ParseNextArrayElement());

                    Attributes[key] = channels;
                }
            }
            //
            // These attributes are string type
            // * directory
            // * filter
            // * lineLogFormat
            //
            else if (_wcsnicmp(key.c_str(), JSON_TAG_DIRECTORY, _countof(JSON_TAG_DIRECTORY)) == 0)
            {
                std::wstring directory = Parser.ParseStringValue();
                FileMonitorUtilities::ParseDirectoryValue(directory);
                //ParseDirectoryValue(directory);
                Attributes[key] = new std::wstring(directory);
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_FILTER, _countof(JSON_TAG_FILTER)) == 0
                || _wcsnicmp(key.c_str(), JSON_TAG_CUSTOM_LOG_FORMAT, _countof(JSON_TAG_CUSTOM_LOG_FORMAT)) == 0)
            {
                Attributes[key] = new std::wstring(Parser.ParseStringValue());
            }
            //
            // These attributes are boolean type
            // * eventFormatMultiLine
            // * startAtOldestRecord
            // * includeSubdirectories
            //
            else if (
                _wcsnicmp(
                    key.c_str(),
                    JSON_TAG_FORMAT_MULTILINE,
                    _countof(JSON_TAG_FORMAT_MULTILINE)) == 0
                || _wcsnicmp(
                    key.c_str(),
                    JSON_TAG_START_AT_OLDEST_RECORD,
                    _countof(JSON_TAG_START_AT_OLDEST_RECORD)) == 0
                || _wcsnicmp(
                    key.c_str(),
                    JSON_TAG_INCLUDE_SUBDIRECTORIES,
                    _countof(JSON_TAG_INCLUDE_SUBDIRECTORIES)) == 0
            )
            {
                Attributes[key] = new bool{ Parser.ParseBooleanValue() };
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_PROVIDERS, _countof(JSON_TAG_PROVIDERS)) == 0)
            {
                if (Parser.GetNextDataType() != JsonFileParser::DataType::Array)
                {
                    LOG(Logger::ERRORS, L"Error parsing configuration file. 'providers' attribute expected to be an array");
                    Parser.SkipValue();
                    continue;
                }

                if (Parser.BeginParseArray())
                {
                    std::vector<ETWProvider>* providers = new std::vector<ETWProvider>();

                    //
                    // Get only the valid providers of this JSON object.
                    //
                    do
                    {
                        providers->emplace_back();
                        if (!ReadETWProvider(Parser, providers->back()))
                        {
                            LOG(Logger::WARNING, L"Error parsing configuration file. Discarded invalid provider (it must have a non-empty 'providerName' or 'providerGuid').");
                            providers->pop_back();
                        }
                    } while (Parser.ParseNextArrayElement());

                    Attributes[key] = providers;
                }
            }
            else if (_wcsnicmp(key.c_str(), JSON_TAG_WAITINSECONDS, _countof(JSON_TAG_WAITINSECONDS)) == 0)
            {
                try
                {
                    auto parsedValue = new std::double_t(Parser.ParseNumericValue());
                    if (*parsedValue < 0)
                    {
                        LOG(Logger::ERRORS, L"Error parsing configuration file. 'waitInSeconds' attribute must be greater or equal to zero");
                        success = false;
                    }
                    else
                    {
                        Attributes[key] = parsedValue;
                    }
                }
                catch(const std::exception& ex)
                {
                    LOG(Logger::ERRORS, Utility::FormatString(L"Error parsing configuration file atrribute 'waitInSeconds'. %S", ex.what()).c_str()
                    );
                    success = false;
                }
            }
            else
            {
                //
                // Discard unwanted attributes
                //
                Parser.SkipValue();
            }
        } while (Parser.ParseNextObjectElement());
    }

    bool isSourceFileValid = ValidateDirectoryAttributes(Attributes);
    if (!isSourceFileValid)
    {
        success = false;
    }

    return success;
}

///
/// Reads a single 'channel' object from the parser, and store it in the Result
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Result       Returns an EventLogChannel struct filled with the values specified in the config file.
///
/// \return True if the channel is valid. Otherwise false
///
bool
ReadLogChannel(
    _In_ JsonFileParser& Parser,
    _Out_ EventLogChannel& Result
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        LOG(Logger::ERRORS, L"Error parsing configuration file. Channel item expected to be an object");
        Parser.SkipValue();
        return false;
    }

    if (!Parser.BeginParseObject())
    {
        LOG(Logger::ERRORS, L"Error parsing configuration file. Error reading channel object");
        return false;
    }

    do
    {
        const auto& key = Parser.GetKey();

        if (_wcsnicmp(key.c_str(), JSON_TAG_CHANNEL_NAME, _countof(JSON_TAG_CHANNEL_NAME)) == 0)
        {
            //
            // Recover the name of the channel
            //
            Result.Name = Parser.ParseStringValue();
        }
        else if (_wcsnicmp(key.c_str(), JSON_TAG_CHANNEL_LEVEL, _countof(JSON_TAG_CHANNEL_LEVEL)) == 0)
        {
            //
            // Get the level as a string, and convert it to EventChannelLogLevel.
            //
            std::wstring logLevelStr = Parser.ParseStringValue();
            bool success = Result.SetLevelByString(logLevelStr);

            //
            // Print an error message the string doesn't matches with a valid log level name.
            //
            if (!success)
            {
                LOG(Logger::ERRORS, Utility::FormatString(
                        L"Error parsing configuration file. '%s' isn't a valid log level. Setting 'Error' level as default", logLevelStr.c_str()
                    ).c_str()
                );
            }
        }
        else
        {
            //
            // Discard unwanted attributes
            //
            Parser.SkipValue();
        }
    } while (Parser.ParseNextObjectElement());


    return Result.IsValid();
}

///
/// Reads a single 'provider' object from the parser, and return it in the Result param
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Result       Returns an ETWProvider struct filled with the values specified in the config file.
///
/// \return True if the channel is valid. Otherwise false
///
bool
ReadETWProvider(
    _In_ JsonFileParser& Parser,
    _Out_ ETWProvider& Result
    )
{
    if (Parser.GetNextDataType() != JsonFileParser::DataType::Object)
    {
        LOG(Logger::ERRORS, L"Error parsing configuration file. Provider item expected to be an object");
        Parser.SkipValue();
        return false;
    }

    if (!Parser.BeginParseObject())
    {
        LOG(Logger::ERRORS, L"Error parsing configuration file. Error reading provider object");
        return false;
    }

    do
    {
        const auto& key = Parser.GetKey();

        if (_wcsnicmp(key.c_str(), JSON_TAG_PROVIDER_NAME, _countof(JSON_TAG_PROVIDER_NAME)) == 0)
        {
            //
            // Recover the name of the provider
            //
            Result.ProviderName = Parser.ParseStringValue();
        }
        else if (_wcsnicmp(key.c_str(), JSON_TAG_PROVIDER_GUID, _countof(JSON_TAG_PROVIDER_GUID)) == 0)
        {
            //
            // Recover the GUID of the provider. If the string isn't a valid
            // GUID, ProviderGuidStr will be empty
            //
            Result.SetProviderGuid(Parser.ParseStringValue());
        }
        else if (_wcsnicmp(key.c_str(), JSON_TAG_PROVIDER_LEVEL, _countof(JSON_TAG_PROVIDER_LEVEL)) == 0)
        {
            //
            // Get the level as a string, and convert it to EventChannelLogLevel.
            //
            std::wstring logLevelStr = Parser.ParseStringValue();
            bool success = Result.StringToLevel(logLevelStr);

            //
            // Print an error message the string doesn't matches with a valid log level name.
            //
            if (!success)
            {
                LOG(Logger::ERRORS, Utility::FormatString(
                        L"Error parsing configuration file. '%s' isn't a valid log level. Setting 'Error' level as default", logLevelStr.c_str()
                    ).c_str()
                );
            }
        }
        else if (_wcsnicmp(key.c_str(), JSON_TAG_KEYWORDS, _countof(JSON_TAG_KEYWORDS)) == 0)
        {
            //
            // Recover the GUID of the provider
            //
            Result.Keywords = wcstoull(Parser.ParseStringValue().c_str(), NULL, 0);
        }
        else
        {
            //
            // Discard unwanted attributes
            //
            Parser.SkipValue();
        }
    } while (Parser.ParseNextObjectElement());


    return Result.IsValid();
}

///
/// Converts the attributes to a valid Source and add it to the vector Sources
///
/// \param Parser       A pre-initialized JSON parser.
/// \param Attributes   An AttributesMap that contains the attributes of the new source objet.
/// \param Sources      A vector, where the new source is going to be inserted after been instantiated.
///
/// \return True if Source was created and added successfully. Otherwise false
///
bool
AddNewSource(
    _In_ JsonFileParser& Parser,
    _In_ AttributesMap& Attributes,
    _Inout_ std::vector<std::shared_ptr<LogSource> >& Sources
    )
{
    //
    // Check the source has a type.
    //
    if (Attributes.find(JSON_TAG_TYPE) == Attributes.end()
        || Attributes[JSON_TAG_TYPE] == nullptr)
    {
        return false;
    }

    switch (*(LogSourceType*)Attributes[JSON_TAG_TYPE])
    {
        case LogSourceType::EventLog:
        {
            std::shared_ptr<SourceEventLog> sourceEventLog = std::make_shared< SourceEventLog>();

            //
            // Fill the new EventLog source object, with its attributes
            //
            if (!SourceEventLog::Unwrap(Attributes, *sourceEventLog))
            {
                LOG(Logger::ERRORS, L"Error parsing configuration file. Invalid EventLog source (it must have a non-empty 'channels')");
                return false;
            }

            Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceEventLog)));

            break;
        }

        case LogSourceType::File:
        {
            std::shared_ptr<SourceFile> sourceFile = std::make_shared< SourceFile>();

            //
            // Fill the new File source object, with its attributes
            //
            if (!SourceFile::Unwrap(Attributes, *sourceFile))
            {
                LOG(Logger::ERRORS, L"Error parsing configuration file. Invalid File source (it must have a non-empty 'directory')");
                return false;
            }

            Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceFile)));

            break;
        }

        case LogSourceType::ETW:
        {
            std::shared_ptr<SourceETW> sourceETW = std::make_shared< SourceETW>();

            //
            // Fill the new ETW source object, with its attributes
            //
            if (!SourceETW::Unwrap(Attributes, *sourceETW))
            {
                LOG(Logger::ERRORS, L"Error parsing configuration file. Invalid ETW source (it must have a non-empty 'providers')");
                return false;
            }

            Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceETW)));

            break;
        }

        case LogSourceType::Process:
        {
            std::shared_ptr<SourceProcess> sourceProcess = std::make_shared< SourceProcess>();

            if (!SourceProcess::Unwrap(Attributes, *sourceProcess))
            {
                LOG(Logger::ERRORS, L"Error parsing configuration file. Invalid Process source)");
                return false;
            }

            Sources.push_back(std::reinterpret_pointer_cast<LogSource>(std::move(sourceProcess)));

            break;
        }
    }
    return true;
}

/*
///
/// originally from bool FileMonitorUtilities::IsValidSourceFile(_In_ std::wstring directory, _In_ bool includeSubdirectories)
/// not to copy the while class file
/// 
//bool FileMonitorUtilities::IsValidSourceFile(_In_ std::wstring directory, _In_ bool includeSubdirectories)
bool IsValidSourceFile(_In_ std::wstring directory, _In_ bool includeSubdirectories)
{
    bool isRootFolder = CheckIsRootFolder(directory);

    // The source file is invalid if the directory is a root folder and includeSubdirectories = true
    // This is because we do not monitor subfolders in the root directory
    return !(isRootFolder && includeSubdirectories);
}
///
/// originally from bool FileMonitorUtilities::CheckIsRootFolder(_In_ std::wstring dirPath)
/// not to copy the while class file
/// 
//bool FileMonitorUtilities::CheckIsRootFolder(_In_ std::wstring dirPath)
bool CheckIsRootFolder(_In_ std::wstring dirPath)
{
    std::wregex pattern(L"^\\w:?$");

    std::wsmatch matches;
    return std::regex_search(dirPath, matches, pattern);
}
*/

///
/// Validates that when root directory is passed, includeSubdirectories is false
///
/// \param Attributes   An AttributesMap that contains the attributes of the new source objet.
/// \return false when root directory is passed, includeSubdirectories = true. Otherwise, true </returns>
bool ValidateDirectoryAttributes(_In_ AttributesMap &Attributes)
{
    if (!Utility::ConfigAttributeExists(Attributes, JSON_TAG_DIRECTORY) ||
        !Utility::ConfigAttributeExists(Attributes, JSON_TAG_INCLUDE_SUBDIRECTORIES))
    {
            return true;
    }

    std::wstring directory = *(std::wstring *)Attributes[JSON_TAG_DIRECTORY];
    const bool includeSubdirectories = *(bool *)Attributes[JSON_TAG_INCLUDE_SUBDIRECTORIES];

    // Check if Log file monitor config is valid
    const bool isValid = FileMonitorUtilities::IsValidSourceFile(directory, includeSubdirectories);
    //const bool isValid = IsValidSourceFile(directory, includeSubdirectories);
    if (!isValid)
    {
        LOG(Logger::ERRORS, Utility::FormatString(
                    L"LoggerSettings: Invalid Source File atrribute 'directory' (%s) and 'includeSubdirectories' (%s)."
                    L"'includeSubdirectories' attribute cannot be 'true' for the root directory",
                    directory.c_str(), includeSubdirectories ? L"true" : L"false")
                    .c_str());
    }
    return isValid;
}

///
/// Debug function
///
void _PrintSettings(_Out_ LoggerSettings& Config)
{
    std::wprintf(L"LogConfig:\n");
    std::wprintf(L"\tsources:\n");

    for (auto source : Config.Sources)
    {
        switch (source->Type)
        {
        case LogSourceType::EventLog:
        {
            std::wprintf(L"\t\tType: EventLog\n");
            std::shared_ptr<SourceEventLog> sourceEventLog = std::reinterpret_pointer_cast<SourceEventLog>(source);

            std::wprintf(L"\t\teventFormatMultiLine: %ls\n", sourceEventLog->EventFormatMultiLine ? L"true" : L"false");
            std::wprintf(L"\t\tstartAtOldestRecord: %ls\n", sourceEventLog->StartAtOldestRecord ? L"true" : L"false");

            std::wprintf(L"\t\tChannels (%d):\n", (int)sourceEventLog->Channels.size());
            for (auto channel : sourceEventLog->Channels)
            {
                std::wprintf(L"\t\t\tName: %ls\n", channel.Name.c_str());
                std::wprintf(L"\t\t\tLevel: %d\n", (int)channel.Level);
                std::wprintf(L"\n");
            }
            std::wprintf(L"\n");

            break;
        }
        case LogSourceType::File:
        {
            std::wprintf(L"\t\tType: File\n");
            std::shared_ptr<SourceFile> sourceFile = std::reinterpret_pointer_cast<SourceFile>(source);

            std::wprintf(L"\t\tDirectory: %ls\n", sourceFile->Directory.c_str());
            std::wprintf(L"\t\tFilter: %ls\n", sourceFile->Filter.c_str());
            std::wprintf(L"\t\tIncludeSubdirectories: %ls\n", sourceFile->IncludeSubdirectories ? L"true" : L"false");
            std::wprintf(L"\t\twaitInSeconds: %d\n", int(sourceFile->WaitInSeconds));
            std::wprintf(L"\n");

            break;
        }
        case LogSourceType::ETW:
        {
            std::wprintf(L"\t\tType: ETW\n");

            std::shared_ptr<SourceETW> sourceETW = std::reinterpret_pointer_cast<SourceETW>(source);

            std::wprintf(L"\t\teventFormatMultiLine: %ls\n", sourceETW->EventFormatMultiLine ? L"true" : L"false");

            std::wprintf(L"\t\tProviders (%d):\n", (int)sourceETW->Providers.size());
            for (auto provider : sourceETW->Providers)
            {
                std::wprintf(L"\t\t\tProviderName: %ls\n", provider.ProviderName.c_str());
                std::wprintf(L"\t\t\tProviderGuid: %ls\n", provider.ProviderGuidStr.c_str());
                std::wprintf(L"\t\t\tLevel: %d\n", (int)provider.Level);
                std::wprintf(L"\t\t\tKeywords: %llx\n", provider.Keywords);
                std::wprintf(L"\n");
            }
            std::wprintf(L"\n");

            break;
        }
        } // Switch
    }
}
