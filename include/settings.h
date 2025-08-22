#pragma once

#include <string>
#include <string_view>

namespace MorphFixer {

    class Settings {
    public:
        static Settings& get() noexcept;
        static constexpr int DEFAULT_THROTTLE_MS = 250;

        // Discoverable paths (wide, to align with SimpleIniW + Windows APIs)
        std::wstring pluginDir() const { return m_plugin_dir; }  // /Data/SKSE/Plugins
        std::wstring selfDir() const { return m_self_dir; }      // /Data/SKSE/Plugins/<dllstem>
        std::wstring iniPath() const { return m_ini_path; }      // resolved ini (preferred in self_dir)

        // User-tunable values (defaults preserved if keys absent)
        int throttle_ms = DEFAULT_THROTTLE_MS;  // delay before applying

        // Load (idempotent). Does not touch other subsystems.
        void load();

        // Utility
        static std::wstring dllStem();
        static std::wstring resolvePluginsDir();
        static std::wstring resolveSelfDir();
        static std::wstring resolveINI();

    private:
        Settings() = default;

        static std::wstring getModulePath();

        std::wstring m_plugin_dir;
        std::wstring m_self_dir;
        std::wstring m_ini_path;
    };

}
