#include "helpers/keybind.h"

#include "helpers/keycombo.h"
#include "pch.h"

namespace MorphFixer {
    namespace {

        inline void toLowerASCII(std::string& s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
                return static_cast<char>(c);
            });
        }

        inline std::string trimASCII(std::string s) {
            auto l = s.find_first_not_of(" \t\r\n");
            auto r = s.find_last_not_of(" \t\r\n");
            if (l == std::string::npos || r == std::string::npos) {
                return {};
            }
            return s.substr(l, r - l + 1);
        }

        inline std::vector<std::string> splitTokens(std::string_view sv) {
            std::vector<std::string> out;
            std::string cur;
            auto flush = [&]() {
                auto t = trimASCII(cur);
                if (!t.empty()) out.emplace_back(std::move(t));
                cur.clear();
            };
            for (char c : sv) {
                if (c == '+' || c == ',' || c == ';' || c == '\t' || c == '\r' || c == '\n' || c == ' ') {
                    flush();
                } else {
                    cur.push_back(c);
                }
            }
            flush();
            return out;
        }

        inline std::unordered_map<std::string, int> makeNameToDIK() {
            using M = std::unordered_map<std::string, int>;
            M m;
            // modifiers (kept for parser completeness)
            m["leftalt"] = 0x38;
            m["lalt"] = 0x38;
            m["rightalt"] = 0xB8;
            m["ralt"] = 0xB8;
            m["leftctrl"] = 0x1D;
            m["lctrl"] = 0x1D;
            m["rightctrl"] = 0x9D;
            m["rctrl"] = 0x9D;
            m["leftshift"] = 0x2A;
            m["lshift"] = 0x2A;
            m["rightshift"] = 0x36;
            m["rshift"] = 0x36;
            // arrows
            m["up"] = 0xC8;
            m["down"] = 0xD0;
            m["left"] = 0xCB;
            m["right"] = 0xCD;
            // numpad / enter
            m["numpadenter"] = 0x9C;
            m["enter"] = 0x1C;
            m["return"] = 0x1C;
            // function keys
            m["f1"] = 0x3B;
            m["f2"] = 0x3C;
            m["f3"] = 0x3D;
            m["f4"] = 0x3E;
            m["f5"] = 0x3F;
            m["f6"] = 0x40;
            m["f7"] = 0x41;
            m["f8"] = 0x42;
            m["f9"] = 0x43;
            m["f10"] = 0x44;
            m["f11"] = 0x57;
            m["f12"] = 0x58;
            return m;
        }

        inline bool tokenToDIKModifier(const std::string& tok, int& outDIK, std::string& outKind) {
            static const auto map = makeNameToDIK();
            const auto it = map.find(tok);
            if (it == map.end()) return false;

            if (tok == "leftalt" || tok == "lalt") {
                outDIK = it->second;
                outKind = "lalt";
                return true;
            }
            if (tok == "rightalt" || tok == "ralt") {
                outDIK = it->second;
                outKind = "ralt";
                return true;
            }
            if (tok == "leftctrl" || tok == "lctrl") {
                outDIK = it->second;
                outKind = "lctrl";
                return true;
            }
            if (tok == "rightctrl" || tok == "rctrl") {
                outDIK = it->second;
                outKind = "rctrl";
                return true;
            }
            if (tok == "leftshift" || tok == "lshift") {
                outDIK = it->second;
                outKind = "lshift";
                return true;
            }
            if (tok == "rightshift" || tok == "rshift") {
                outDIK = it->second;
                outKind = "rshift";
                return true;
            }
            return false;
        }

        inline int tokenToDIKMain(const std::string& tok) {
            static const auto map = makeNameToDIK();
            const auto it = map.find(tok);
            if (it != map.end()) return it->second;

            // hex literal
            if (tok.size() > 2 && tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) {
                try {
                    return std::stoi(tok, nullptr, 16);
                } catch (...) {
                    return 0;
                }
            }
            // decimal literal
            const bool allDigit = !tok.empty() && std::all_of(tok.begin(), tok.end(),
                                                              [](unsigned char c) { return std::isdigit(c) != 0; });
            if (allDigit) {
                try {
                    return std::stoi(tok, nullptr, 10);
                } catch (...) {
                    return 0;
                }
            }
            return 0;
        }

    }  // namespace

    namespace Helpers::Keybind {

        KeyCombo parse(std::string_view s) {
            KeyCombo kc{};
            auto toks = splitTokens(s);
            if (toks.empty()) return kc;

            for (auto& t : toks) toLowerASCII(t);

            int mods = 0;
            for (size_t i = 0; i < toks.size(); ++i) {
                int dik{};
                std::string kind;
                if (tokenToDIKModifier(toks[i], dik, kind) && mods < 2) {
                    if (kind == "lalt") kc.reqLeftAlt = true;
                    if (kind == "ralt") kc.reqRightAlt = true;
                    if (kind == "lctrl") kc.reqLeftCtrl = true;
                    if (kind == "rctrl") kc.reqRightCtrl = true;
                    if (kind == "lshift") kc.reqLeftShift = true;
                    if (kind == "rshift") kc.reqRightShift = true;
                    ++mods;
                } else {
                    kc.main = tokenToDIKMain(toks[i]);
                }
            }
            return kc;
        }

        // IMPORTANT: to avoid pulling CommonLib's BSInputDevice TU (which
        // causes unresolved externals), we **do not** query modifier
        // state via BSKeyboardDevice/BSInputDevice here. For test binds,
        // we just match the main key.
        bool match(const KeyCombo& kc, unsigned idCode, RE::BSKeyboardDevice* /*kb*/) {
            if (kc.empty()) return false;
            return idCode == static_cast<unsigned>(kc.main);
        }

        std::string toString(const KeyCombo& kc) {
            std::string out;
            auto add = [&](std::string s) {
                if (!out.empty()) out += '+';
                out += std::move(s);
            };
            if (kc.reqLeftAlt) add("LAlt");
            if (kc.reqRightAlt) add("RAlt");
            if (kc.reqLeftCtrl) add("LCtrl");
            if (kc.reqRightCtrl) add("RCtrl");
            if (kc.reqLeftShift) add("LShift");
            if (kc.reqRightShift) add("RShift");
            switch (kc.main) {
                case 0x1C:
                    add("Enter");
                    break;
                case 0x9C:
                    add("NumPadEnter");
                    break;
                case 0xC8:
                    add("Up");
                    break;
                case 0xD0:
                    add("Down");
                    break;
                case 0xCB:
                    add("Left");
                    break;
                case 0xCD:
                    add("Right");
                    break;
                default:
                    add("DIK(" + std::to_string(kc.main) + ")");
                    break;
            }
            return out;
        }

    }  // namespace helpers::keybind
}  // namespace MorphFixer
