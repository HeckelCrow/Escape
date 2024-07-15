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
        SCOPE_EXIT({ CloseHandle(file); });
        LARGE_INTEGER file_size;

        if (!GetFileSizeEx(file, &file_size))
        {
            PrintError("Error! Cannot get the file size!\n");
            return {};
        }
        if (file_size.HighPart) // Can't deal with files that big yet
        {
            PrintError("Error! file is too big ({} bytes)!\n",
                       file_size.QuadPart);
            return {};
        }
        data.resize(file_size.LowPart);

        DWORD bytes_read;
        if (ReadFile(file, &data[0], file_size.LowPart, &bytes_read, NULL))
        {
            if (bytes_read != file_size.QuadPart)
            {
                PrintWarning(
                    "Couldn't read the whole file. Expected {} but read {}\n",
                    file_size.QuadPart, bytes_read);
            }
        }
        else
        {
            PrintError("ReadFile failed (error: {})\n", GetLastError());
            data.clear();
        }
    }
    return data;
}

bool
WriteFile(const WStr& path_16, StrPtr data, DWORD access, DWORD creation)
{
    bool   success = false;
    HANDLE file    = CreateFileW(path_16.c_str(), access, FILE_SHARE_READ, NULL,
                                 creation, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        SCOPE_EXIT({ CloseHandle(file); });

        DWORD bytes_written;
        if (WriteFile(file, &data[0], data.size(), &bytes_written, NULL))
        {
            if (bytes_written == data.size())
            {
                success = true;
            }
            else
            {
                PrintWarning(
                    "Couldn't write the whole file. Expected {} but read {}\n",
                    data.size(), bytes_written);
            }
        }
        else
        {
            PrintError("WriteFile failed (error: {})\n", GetLastError());
        }
    }
    else
    {
        PrintError("CreateFileW failed (error: {})\n", GetLastError());
    }
    return success;
}

bool
WriteFile(const Path& path, StrPtr data)
{
    return WriteFile(path.wstring(), data, GENERIC_WRITE, CREATE_ALWAYS);
}

bool
AppendToFile(const Path& path, StrPtr data)
{
    return WriteFile(path.wstring(), data, FILE_APPEND_DATA, OPEN_ALWAYS);
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
