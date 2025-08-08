#include "string.h"

#include <Windows.h>

namespace {
    inline bool is_ascii_space(char c) noexcept
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }
}

namespace helpers::string {

    std::string toUtf8(std::wstring_view ws)
    {
        if (ws.empty()) {
            return {};
        }
        const int needed =
            ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
        if (needed <= 0) {
            return {};
        }
        std::string out(needed, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), needed, nullptr, nullptr);
        return out;
    }

    std::vector<std::string> splitList(std::string_view s)
    {
        std::vector<std::string> out;
        if (s.empty()) return out;

        std::size_t tokStart = 0;
        const auto flush = [&](std::size_t end) {
            // trim ASCII
            std::size_t l = tokStart;
            std::size_t r = end;
            while (l < r && is_ascii_space(s[l])) ++l;
            while (r > l && is_ascii_space(s[r - 1])) --r;
            if (r > l) {
                out.emplace_back(s.substr(l, r - l));
            }
        };

        for (std::size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];
            if (c == ',' || c == ';') {
                flush(i);
                tokStart = i + 1;
            }
        }
        flush(s.size()); // last token
        return out;
    }
}
