//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//

#pragma once

class JsonFileParser
{
public:
    JsonFileParser(_In_ const wchar_t* BufferStart, size_t BufferLength)
        : m_buffer(BufferStart),
          m_bufferLength(BufferLength)
    {
        //
        // Skip whitespace characters
        //
        AdvanceBufferPointer(0);
    }

    JsonFileParser(_In_ const std::wstring& JsonString)
        : JsonFileParser(JsonString.data(), JsonString.size())
    {
    }

    enum class DataType
    {
        Array,
        Object,
        Boolean,
        String,
        Number,
        Null,
    };

    DataType GetNextDataType();

    const std::wstring& ParseStringValue();
    const std::double_t& ParseNumericValue();
    const std::double_t& ParseNumber();

    bool ParseBooleanValue();

    void ParseNullValue();

    bool BeginParseArray();

    bool ParseNextArrayElement();

    bool BeginParseObject();

    const std::wstring& GetKey();

    bool ParseNextObjectElement();

    void SkipValue();

private:
    //
    // Buffer to parse
    //
    const wchar_t* m_buffer;

    size_t m_bufferLength;

    //
    // Current position in a buffer to parse
    //
    size_t m_currentPos = 0;

    //
    // Last parsed object key
    //
    std::wstring m_key;

    //
    // Last parsed string value
    //
    std::wstring m_stringValue;

    //
    // Last parsed numeric value
    //
    std::double_t m_doubleValue;

    static const int EndOfBuffer = -1;

    static inline int HexToInt(
        _In_ int HexChar
        );

    static inline bool IsWhiteSpace(
        _In_ int Character
        );

    static inline bool ContainsInfinitySymbol(
        _In_ std::wstring text
        );

    static inline wchar_t ParseSpecialCharacter(int Character);

    wchar_t ParseControlCharacter(size_t Offset);

    void ParseKey();

    int PeekNextCharacter(
        size_t Offset = 0
        );

    void AdvanceBufferPointer(
        size_t Offset
        );

    void SkipNumberValue();
};
