#pragma once
#include "alias.hpp"

Str  ReadBinaryFile(const Path& path);
void WriteFile(const Path& path, StrPtr data);

Str  WideCharToUtf8(const wchar_t* wide, s32 count);
WStr Utf8ToWideChar(StrPtr path);