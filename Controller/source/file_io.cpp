#include "file_io.hpp"
#include "print.hpp"
#include "scope_exit.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOUSER
#include <Windows.h>
#include <assert.h>

Str
ReadBinaryFile(const Path& path)
{
    Str    data;
    WStr   path_16 = path.wstring();
    HANDLE file    = CreateFileW(path_16.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                 NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER file_size;

        if (!GetFileSizeEx(file, &file_size))
        {
            PrintError("Error! Cannot get the file size!\n");
            CloseHandle(file);
            return data;
        }
        assert(!file_size.HighPart); // Can't deal with files that big yet

        data.resize(file_size.LowPart);

        DWORD bytes_read;
        if (ReadFile(file, &data[0], file_size.LowPart, &bytes_read, NULL))
        {
            assert(bytes_read == file_size.QuadPart);
        }
        else
        {
            PrintError("ReadFile failed (error: {})\n", GetLastError());
            data.clear();
        }
        CloseHandle(file);
    }
    return data;
}

enum class UnicodeType
{
    Unknown,
    UTF_8,
    UTF_16LE,
    UTF_16BE,
    Count
};

char g_unicode_type_names[(u32)UnicodeType::Count][256] = {
    "Unknown", "UTF-8", "UTF-16LE", "UTF-16BE"};

inline u32
GetUnicodeBomSize(UnicodeType type)
{
    switch (type)
    {
    case UnicodeType::UTF_8: return 3;
    case UnicodeType::UTF_16LE: return 2;
    case UnicodeType::UTF_16BE: return 2;
    case UnicodeType::Unknown:
    default: return 0;
    }
}

UnicodeType
DetectUnicodeType(u8* buffer, u32 size)
{
    if (size >= GetUnicodeBomSize(UnicodeType::UTF_8) && buffer[0] == 0xEF
        && buffer[1] == 0xBB && buffer[2] == 0xBF)
    {
        return UnicodeType::UTF_8;
    }
    if (size >= GetUnicodeBomSize(UnicodeType::UTF_16LE) && buffer[0] == 0xFF
        && buffer[1] == 0xFE)
    {
        return UnicodeType::UTF_16LE;
    }
    if (size >= GetUnicodeBomSize(UnicodeType::UTF_16BE) && buffer[0] == 0xFE
        && buffer[1] == 0xFF)
    {
        return UnicodeType::UTF_16BE;
    }
    return UnicodeType::Unknown;
}

void
RemoveUnicodeBom(Str& str, UnicodeType type)
{
    u32 bom_size = GetUnicodeBomSize(type);
    str.erase(0, bom_size);
}

Str
ReadUtf8File(const Path& path)
{
    Str ret;

    WStr   wpath = path.wstring();
    HANDLE file  = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        SCOPE_EXIT({ CloseHandle(file); });

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(file, &file_size))
        {
            PrintError("Error! Cannot get the file size!\n");
            return ret;
        }
        assert(!file_size.HighPart); // Can't deal with files that big yet
        char* buffer;
        u32   buffer_capacity = file_size.LowPart + 2;
        buffer                = (char*)malloc(buffer_capacity);
        assert(buffer);
        buffer[file_size.LowPart + 0] = '\0';
        buffer[file_size.LowPart + 1] = '\0';
        DWORD bytes_read;
        if (ReadFile(file, buffer, file_size.LowPart, &bytes_read, NULL))
        {
            UnicodeType unicode_type =
                DetectUnicodeType((u8*)buffer, bytes_read);
            switch (unicode_type)
            {
            case UnicodeType::UTF_8: {
                // Print("Detected UTF-8\n");
                ret = buffer;
            }
            break;
            case UnicodeType::UTF_16LE: {
                // Print("Detected UTF-16LE\n");
                auto utf8_size = WideCharToMultiByte(
                    CP_UTF8, 0, (wchar_t*)buffer, bytes_read / sizeof(wchar_t),
                    NULL, 0, NULL, NULL);
                assert(utf8_size);
                ret.resize(utf8_size);
                auto ret_size = WideCharToMultiByte(
                    CP_UTF8, 0, (wchar_t*)buffer, bytes_read / sizeof(wchar_t),
                    &ret[0], utf8_size, NULL, NULL);
                if (!ret_size)
                {
                    PrintError("Cannot convert file to UTF-8\n");
                }
            }
            break;
            case UnicodeType::UTF_16BE: {
                // Print("Detected UTF-16BE\n");
                // Flip every bytes to create UTF-16LE
                WCHAR* ptr = (WCHAR*)buffer;
                while (*ptr)
                {
                    *ptr = (((*ptr) << 8) & 0xFF00) + (((*ptr) >> 8) & 0x00FF);
                    ptr++;
                }
                // Convert to UTF-8
                auto utf8_size = WideCharToMultiByte(
                    CP_UTF8, 0, (wchar_t*)buffer, bytes_read / sizeof(wchar_t),
                    NULL, 0, NULL, NULL);
                assert(utf8_size);
                ret.resize(utf8_size);
                auto ret_size = WideCharToMultiByte(
                    CP_UTF8, 0, (wchar_t*)buffer, bytes_read / sizeof(wchar_t),
                    &ret[0], utf8_size, NULL, NULL);
                if (!ret_size)
                {
                    PrintError("Cannot convert file to UTF-8\n");
                }
            }
            break;
            case UnicodeType::Unknown: {
                // Let's hope it's UTF8 already
                ret = buffer;

                //// Print("Didn't detect the encoding, using ANSI\n");
                //// Convert to UTF-16
                // auto utf16_size = MultiByteToWideChar(
                //     CP_ACP, MB_ERR_INVALID_CHARS, buffer, bytes_read, NULL,
                //     0);
                // assert(utf16_size);
                // WCHAR* buffer_utf16 =
                //     (WCHAR*)malloc(utf16_size * sizeof(WCHAR));
                // assert(buffer_utf16);
                // utf16_size =
                //     MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, buffer,
                //                         bytes_read, buffer_utf16,
                //                         utf16_size);
                // if (!utf16_size)
                //{
                //     print("Cannot convert file from ANSI to UTF-16\n");
                // }
                //// Convert to UTF-8
                // auto utf8_size = WideCharToMultiByte(
                //     CP_UTF8, 0, buffer_utf16, utf16_size, NULL, 0, NULL,
                //     NULL);
                // assert(utf8_size);
                // ret.resize(utf8_size);
                // auto ret_size =
                //     WideCharToMultiByte(CP_UTF8, 0, buffer_utf16, utf16_size,
                //                         &ret[0], utf8_size, NULL, NULL);
                // free(buffer_utf16);
                // if (!ret_size)
                //{
                //     print("Cannot convert file from UTF-16 to UTF-8\n");
                // }
            }
            break;
            default:
                ret = buffer;
                assert(false);
                break;
            }
            RemoveUnicodeBom(ret, unicode_type);
            // Print("Converted data:\n{}\n\n", data);
        }
        free(buffer);
    }
    else
    {
        PrintError("Cannot open file \"{}\" (error: {})\n", path.string(),
                   GetLastError());
    }
    return ret;
}

Str
WideCharToUtf8(const wchar_t* wide, s32 count)
{
    std::string utf8;
    if (!count)
    {
        return "";
    }
    s32 size_needed =
        WideCharToMultiByte(CP_UTF8, 0, wide, count, nullptr, 0, NULL, NULL);
    utf8.resize(size_needed);
    if (!size_needed)
    {
        // This shouldn't happen
        return "";
    }
    WideCharToMultiByte(CP_UTF8, 0, wide, count, &utf8[0], size_needed, NULL,
                        NULL);
    if (count < 0)
        utf8.resize(utf8.size() - 1); // remove '\0'
    return utf8;
}

WStr
Utf8ToWideChar(StrPtr utf8)
{
    WStr wstr;
    if (utf8.empty())
    {
        return wstr;
    }
    s32 size_needed =
        MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    wstr.resize(size_needed);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &wstr[0],
                        size_needed);
    return wstr;
}
