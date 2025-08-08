#pragma once

#include <string>
#include <string_view>

class Settings {
public:
    static Settings& get() noexcept;

    // Discoverable paths (wide, to align with SimpleIniW + Windows APIs)
    std::wstring plugin_dir() const { return plugin_dir_; }  // .../Data/SKSE/Plugins
    std::wstring self_dir()   const { return self_dir_; }    // .../Data/SKSE/Plugins/<dllstem>
    std::wstring ini_path()   const { return ini_path_; }    // resolved ini (preferred in self_dir)

    // User-tunable values (defaults preserved if keys absent)
    int   throttle_ms       = 250;  // delay before applying

    // Load (idempotent). Does not touch other subsystems.
    void load();

    // Utility (kept public as in your sketch)
    static std::wstring dllStem();
    static std::wstring resolvePluginsDir();
    static std::wstring resolveSelfDir();
    static std::wstring resolveINI();

private:
    Settings() = default;

    static std::wstring getModulePath();

    std::wstring plugin_dir_;
    std::wstring self_dir_;
    std::wstring ini_path_;
};
