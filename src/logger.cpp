

// logger.cpp
#include "logger.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>  // fallback for DebugView/Output window

#include <filesystem>

#include "pch.h"

#ifndef PLUGIN_NAME
    #define PLUGIN_NAME "RacemenuMorphFixer"
#endif

namespace MorphFixer {
    namespace Logger {
        void init() {
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("%s logger init", PLUGIN_NAME);
            }
            const auto dir = SKSE::log::log_directory();
            if (!dir) {
                // As a last resort, log to the debugger so you see *something*
                auto dbg = std::make_shared<spdlog::sinks::msvc_sink_mt>();
                auto logger = std::make_shared<spdlog::logger>("dbg", dbg);
                spdlog::set_default_logger(logger);
                spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
                spdlog::warn("log_directory() is null; are we too early?");
                return;
            }

            const auto path = *dir / (std::string{PLUGIN_NAME} + ".log");
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);  // ignore error if exists

            try {
                auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
                auto logger = std::make_shared<spdlog::logger>("file", file_sink);
                spdlog::set_default_logger(std::move(logger));
                spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
                // Make sure debug logs actually show up:
                spdlog::set_level(spdlog::level::debug);       // runtime threshold
                spdlog::flush_on(spdlog::level::debug);        // flush on every debug+
                spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");  // optional

                spdlog::info("Logger initialized at: {}", path.string());
            } catch (const std::exception& e) {
                // Fallback to debugger sink if file sink failed
                auto dbg = std::make_shared<spdlog::sinks::msvc_sink_mt>();
                auto logger = std::make_shared<spdlog::logger>("dbg", dbg);
                spdlog::set_default_logger(logger);
                spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
                spdlog::error("Failed to open log file '{}': {}", path.string(), e.what());
            }
        }
    }
}  // namespace MorphFixer
