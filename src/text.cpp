#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700  // for wcwidth
#endif

#include "console_pet/text.hpp"

#include <cstdint>
#include <cwchar>

namespace pet {
namespace {

/// Append a single code point, emitting UTF-16 surrogates when wchar_t is only
/// 16 bits wide (e.g. Windows). On 32-bit wchar_t platforms this is a no-op
/// beyond the direct push.
void append_code_point(std::wstring& out, std::uint32_t cp) {
    if constexpr (sizeof(wchar_t) >= 4) {
        out.push_back(static_cast<wchar_t>(cp));
    } else {
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<wchar_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
}

}  // namespace

std::wstring to_wide(std::string_view utf8) {
    std::wstring out;
    out.reserve(utf8.size());

    std::size_t i = 0;
    while (i < utf8.size()) {
        auto c = static_cast<unsigned char>(utf8[i]);
        std::uint32_t cp = 0;
        int extra = 0;

        if (c < 0x80) {                 // 0xxxxxxx
            cp = c;
        } else if ((c >> 5) == 0x6) {   // 110xxxxx
            cp = c & 0x1F;
            extra = 1;
        } else if ((c >> 4) == 0xE) {   // 1110xxxx
            cp = c & 0x0F;
            extra = 2;
        } else if ((c >> 3) == 0x1E) {  // 11110xxx
            cp = c & 0x07;
            extra = 3;
        } else {
            ++i;  // invalid lead byte; skip it
            continue;
        }

        ++i;
        for (int k = 0; k < extra && i < utf8.size(); ++k, ++i) {
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i]) & 0x3F);
        }
        append_code_point(out, cp);
    }
    return out;
}

std::size_t codepoint_count(std::string_view utf8) {
    std::size_t count = 0;
    for (auto c : utf8) {
        // Count bytes that are NOT continuation bytes (10xxxxxx).
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

std::string to_utf8(wchar_t ch) {
    std::uint32_t cp;
    if constexpr (sizeof(wchar_t) >= 4) {
        cp = static_cast<std::uint32_t>(ch);
    } else {
        cp = static_cast<std::uint16_t>(ch);  // surrogate handling omitted
    }
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

int display_width(std::string_view utf8) {
    int width = 0;
    for (wchar_t ch : to_wide(utf8)) {
        const int cw = wcwidth(ch);
        if (cw > 0) width += cw;  // non-spacing (0) and control (-1) add nothing
    }
    return width;
}

std::string pad_cells(std::string_view utf8, int target) {
    std::string out(utf8);
    for (int w = display_width(utf8); w < target; ++w) out += ' ';
    return out;
}

void pop_codepoint(std::string& s) {
    if (s.empty()) return;
    std::size_t i = s.size() - 1;
    // Walk back over UTF-8 continuation bytes (10xxxxxx) to the lead byte.
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
    s.erase(i);
}

}  // namespace pet
