#include "helpers/string.h"

#include <Windows.h>

namespace MorphFixer {

    namespace {
        bool isAsciiSpace(const char c) noexcept { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
    }

    namespace Helpers::String {

        std::string toUtf8(const std::wstring_view ws) {
            if (ws.empty()) {
                return {};
            }
            const int needed =
                ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
            if (needed <= 0) {
                return {};
            }
            std::string out(needed, '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), needed, nullptr,
                                  nullptr);
            return out;
        }

        std::vector<std::string> splitList(const std::string_view input_string) {
            std::vector<std::string> out;
            if (input_string.empty()) {
                return out;
            }

            std::size_t tok_start = 0;
            const auto flush = [&](const std::size_t end) {
                // trim ASCII
                std::size_t left_idx = tok_start;
                std::size_t right_idx = end;
                while (left_idx < right_idx && isAsciiSpace(input_string[left_idx])) {
                    ++left_idx;
                }
                while (right_idx > left_idx && isAsciiSpace(input_string[right_idx - 1])) {
                    --right_idx;
                }
                if (right_idx > left_idx) {
                    out.emplace_back(input_string.substr(left_idx, right_idx - left_idx));
                }
            };

            for (std::size_t i = 0; i < input_string.size(); ++i) {
                if (const char current_char = input_string[i]; current_char == ',' || current_char == ';') {
                    flush(i);
                    tok_start = i + 1;
                }
            }
            flush(input_string.size());  // last token
            return out;
        }
    }
}  // namespace MorphFixer
