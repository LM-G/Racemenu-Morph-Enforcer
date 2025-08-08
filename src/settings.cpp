#include "settings.h"

#include <SimpleIni.h>
#include <windows.h>  // GetModuleHandleExW, GetModuleFileNameW

#include <cwctype>
#include <filesystem>
#include <string>

#include "helpers/string.h"
#include "logger.h"
#include "pch.h"

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

using std::filesystem::path;

Settings& Settings::get() noexcept {
    static Settings s;
    return s;
}

std::wstring Settings::getModulePath() {
    wchar_t buf[MAX_PATH]{};
    HMODULE h{};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&Settings::get), &h)) {
        GetModuleFileNameW(h, buf, MAX_PATH);
    }
    return buf;
}

std::wstring Settings::dllStem() {
    const auto full = getModulePath();
    path p(full);
    auto stem = p.stem().wstring();
    // ensure lowercase file names
    for (auto& ch : stem) ch = static_cast<wchar_t>(std::towlower(ch));
    return stem;
}

std::wstring Settings::resolvePluginsDir() {
    // Assumes plugin DLL is at .../Data/SKSE/Plugins/<dll>.dll
    const auto full = getModulePath();
    path p(full);
    // .../Data/SKSE/Plugins
    p = p.parent_path();
    return p.wstring();
}

std::wstring Settings::resolveSelfDir() {
    const auto dir = resolvePluginsDir();
    return (path(dir) / WIDEN(PLUGIN_NAME)).wstring();
}

std::wstring Settings::resolveINI() {
    const auto self = resolveSelfDir();
    const auto plug = resolvePluginsDir();

    // keep lowercase file name based on dll stem
    const auto stem = dllStem() + L".ini";

    const path preferred = path(self) / stem;  // .../Plugins/RacemenuMorphFixer/racemenumorphfixer.ini
    if (std::filesystem::exists(preferred)) return preferred.wstring();

    // fallback: .../Plugins/racemenumorphfixer.ini (flat)
    return (path(plug) / stem).wstring();
}

void Settings::load() {
    plugin_dir_ = resolvePluginsDir();
    self_dir_ = resolveSelfDir();
    ini_path_ = resolveINI();

    CSimpleIniW ini;
    ini.SetUnicode();
    const auto rc = ini.LoadFile(ini_path_.c_str());
    if (rc < 0) {
        LOG_INFO("[config] INI not found, using defaults (throttle_ms={})", throttle_ms);
        return;
    }

    // ---- general (section names chosen to keep room for future options) ----
    throttle_ms = static_cast<int>(ini.GetLongValue(L"delays", L"throttle_ms", throttle_ms));

    LOG_INFO("[config] loaded '{}' (throttle_ms={})", helpers::string::toUtf8(ini_path_), throttle_ms);
}
