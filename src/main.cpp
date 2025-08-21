#include <SimpleIni.h>

#include "arrow_weight_sink.h"
#include "helpers/keybind.h"
#include "logger.h"
#include "morph_updater.h"
#include "pch.h"
#include "racemenu_watcher.h"
#include "settings.h"
#include "skee.h"

static void loadMorphInterface() {
    if (!GetModuleHandleW(L"skee64.dll") && !GetModuleHandleW(L"skee.dll")) {
        LOG_WARN("[SKEE] skee DLL not loaded, skipping");
        return;
    }

    auto* mi = SKSE::GetMessagingInterface();
    if (!mi) {
        LOG_WARN("[SKEE] MessagingInterface is null");
        return;
    }

    SKEE::InterfaceExchangeMessage msg{};
    if (!mi->Dispatch(SKEE::InterfaceExchangeMessage::kExchangeInterface, &msg, sizeof(SKEE::InterfaceExchangeMessage*),
                      "skee")) {
        LOG_WARN("[SKEE] Dispatch to skee failed");
        return;
    }

    if (!msg.interfaceMap) {
        LOG_DEBUG("[SKEE] interfaceMap == nullptr (RaceMenu not ready yet)");
        return;
    }

    const auto morphInterface = static_cast<SKEE::IBodyMorphInterface*>(msg.interfaceMap->QueryInterface("BodyMorph"));
    if (!morphInterface) {
        LOG_CRITICAL("Couldn't get serialization MorphInterface!");
        return;
    }

    LOG_INFO("[SKEE] BodyMorph version {}", morphInterface->GetVersion());
    morph_updater::get().setMorphInterface(morphInterface);
}

static void onDataLoaded() {
     Settings::get().load();

     morph_updater::get().set_throttle_ms(Settings::get().throttle_ms);

     if (auto* ui = RE::UI::GetSingleton()) {
         ui->AddEventSink<RE::MenuOpenCloseEvent>(&racemenu_watcher::get());
     }
     LOG_INFO("DataLoaded handled; menu watcher attached.");

    if (auto* console = RE::ConsoleLog::GetSingleton()) {
        LOG_INFO(PLUGIN_NAME ": Loaded!");
    }

    LOG_INFO(PLUGIN_NAME ": DataLoaded handled.");
}

static void install_weight_test_binds() {
    static std::atomic<bool> s_installed{false};
    if (s_installed.exchange(true)) {
        return;  // already installed
    }
    if (auto* mgr = RE::BSInputDeviceManager::GetSingleton()) {
        mgr->AddEventSink(&ArrowWeightSink::get());
        LOG_INFO("[keybind] Arrow Up/Down mapped to 100/0");
    } else {
        LOG_WARN("[keybind] BSInputDeviceManager not available; cannot install binds");
    }
}

static void onMessage(SKSE::MessagingInterface::Message* m) {
    switch (m->type) {
        case SKSE::MessagingInterface::kPostLoad: {
            // nothing
        } break;
        case SKSE::MessagingInterface::kPostPostLoad: {
            loadMorphInterface();
        } break;
        case SKSE::MessagingInterface::kDataLoaded: {
            onDataLoaded();
        } break;
        case SKSE::MessagingInterface::kInputLoaded: {
            // nothing
            //install_weight_test_binds();
        } break;
        case SKSE::MessagingInterface::kPreLoadGame: {
            // nothing
        } break;
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame: {
            Settings::get().load();
        } break;
        default:
            break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    logx::init();
    SKSE::GetMessagingInterface()->RegisterListener(onMessage);
    spdlog::info(PLUGIN_NAME " loaded.");
    return true;
}