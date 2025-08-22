#include "settings.h"

#include <SimpleIni.h>
#include <windows.h>  // GetModuleHandleExW, GetModuleFileNameW

#include "helpers/string.h"
#include "logger.h"
#include "pch.h"

namespace MorphFixer {

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

    using std::filesystem::path;

    Settings& Settings::get() noexcept {
        static Settings instance;
        return instance;
    }

    std::wstring Settings::getModulePath() {
        wchar_t buf[MAX_PATH]{};
        HMODULE h_module{};
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&Settings::get), &h_module) != 0) {
            GetModuleFileNameW(h_module, buf, MAX_PATH);
        }
        return buf;
    }

    std::wstring Settings::dllStem() {
        const auto full = getModulePath();
        const path full_path(full);
        auto stem = full_path.stem().wstring();
        // ensure lowercase file names
        for (auto& wchar : stem) {
            wchar = static_cast<wchar_t>(std::towlower(wchar));
        }
        return stem;
    }

    std::wstring Settings::resolvePluginsDir() {
        // Assumes plugin DLL is at /Data/SKSE/Plugins/<dll>.dll
        const auto full = getModulePath();
        path full_path(full);
        // /Data/SKSE/Plugins
        full_path = full_path.parent_path();
        return full_path.wstring();
    }

    std::wstring Settings::resolveSelfDir() {
        const auto dir = resolvePluginsDir();
        return (path(dir) / WIDEN(PLUGIN_NAME)).wstring();
    }

    std::wstring Settings::resolveINI() {
        const auto self = resolveSelfDir();
        const auto plug = resolvePluginsDir();

        // keep a lowercase file name based on dll stem
        const auto stem = dllStem() + L".ini";

        const path preferred = path(self) / stem;  // /Plugins/RacemenuMorphFixer/racemenumorphfixer.ini
        if (std::filesystem::exists(preferred)) {
            return preferred.wstring();
        }

        // fallback: /Plugins/racemenumorphfixer.ini (flat)
        return (path(plug) / stem).wstring();
    }

    void Settings::load() {
        m_plugin_dir = resolvePluginsDir();
        m_self_dir = resolveSelfDir();
        m_ini_path = resolveINI();

        CSimpleIniW ini;
        ini.SetUnicode();

        if (const auto status = ini.LoadFile(m_ini_path.c_str()); status < 0) {
            LOG_INFO("[config] INI not found, using defaults (throttle_ms={})", throttle_ms);
            return;
        }

        // ---- general (section names chosen to keep room for future options) ----
        throttle_ms = static_cast<int>(ini.GetLongValue(L"delays", L"throttle_ms", throttle_ms));

        LOG_INFO("[config] loaded '{}' (throttle_ms={})", Helpers::String::toUtf8(m_ini_path), throttle_ms);
    }
}  // namespace MorphFixer
