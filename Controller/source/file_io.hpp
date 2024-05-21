#pragma once
#include "alias.hpp"

Str ReadBinaryFile(const Path& path);
Str ReadUtf8File(const Path& path);

Str  WideCharToUtf8(const wchar_t* wide, s32 count);
WStr Utf8ToWideChar(StrPtr path);