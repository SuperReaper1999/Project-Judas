#pragma once

#include <string>

// Quote one argument for the Windows CRT argv parser. This does not invoke a
// shell: backslashes are doubled only before quotes and the closing quote.
// Keep this platform-independent so edge cases can be checked on every host.
inline std::wstring QuoteWindowsProcessArgument(const std::wstring& text) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const wchar_t ch : text) {
        if (ch == L'\\') { ++slashes; continue; }
        result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        result += ch;
    }
    result.append(slashes * 2, L'\\');
    result += L'"';
    return result;
}
