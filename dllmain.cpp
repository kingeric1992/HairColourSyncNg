#include "plugin.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

//AE
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = []() {
    SKSE::PluginVersionData v{};
    v.pluginVersion = PACKED_VERSION;
    v.PluginName(PLUGIN_NAME);
    v.AuthorName(AUTHOR);
    v.AuthorEmail(EMAIL);
    v.UsesAddressLibrary();
    v.UsesNoStructs();
    return v;
    }();
//SE
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface* skse, SKSE::PluginInfo* info)
{
    info->infoVersion   = SKSEPlugin_Version.kVersion;
    info->name          = SKSEPlugin_Version.pluginName;
    info->version       = SKSEPlugin_Version.pluginVersion;
    return !skse->IsEditor();
}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse, false);
    SKSE::AllocTrampoline(32); // 14 + 10

	hook(); // fatal error would exit the game, no need to check return value

    return true;
}
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);
#ifdef _DEBUG
        while (!IsDebuggerPresent()) Sleep(100);

        if (auto path = SKSE::log::log_directory()) {

            *path /= std::format("{}.log", PLUGIN_NAME);

            std::vector<spdlog::sink_ptr> sinks{
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true),
                std::make_shared<spdlog::sinks::msvc_sink_mt>()
            };
            auto logger = std::make_shared<spdlog::logger>("global", sinks.begin(), sinks.end());
#	ifndef NDEBUG
            logger->set_level(spdlog::level::debug);
            logger->flush_on(spdlog::level::debug);
#	else
            logger->set_level(spdlog::level::info);
            logger->flush_on(spdlog::level::err);
#	endif
            spdlog::set_default_logger(std::move(logger));
            spdlog::set_pattern("[%T.%e] [%=5t] [%L] %v");
        }
        //}();
        SKSE::log::info("{} v{}", SKSEPlugin_Version.pluginName, REL::Version::unpack(SKSEPlugin_Version.pluginVersion));
#endif
    } break;
    }
    return TRUE;
}