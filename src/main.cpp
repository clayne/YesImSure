#include "skse64_common/BranchTrampoline.h"  // g_localTrampoline, g_branchTrampoline
#include "skse64_common/skse_version.h"  // RUNTIME_VERSION

#include "Hooks.h"  // InstallHooks
#include "Settings.h"  // Settings
#include "version.h"  // VERSION_VERSTRING, VERSION_MAJOR

#include "SKSE/API.h"


extern "C" {
	bool SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
	{
		SKSE::Logger::OpenRelative(FOLDERID_Documents, L"\\My Games\\Skyrim Special Edition\\SKSE\\FileCacheSSE.log");
		SKSE::Logger::SetPrintLevel(SKSE::Logger::Level::kDebugMessage);
		SKSE::Logger::SetFlushLevel(SKSE::Logger::Level::kDebugMessage);

		_MESSAGE("YesImSure v%s", YISR_VERSION_VERSTRING);

		a_info->infoVersion = SKSE::PluginInfo::kVersion;
		a_info->name = "YesImSure";
		a_info->version = YISR_VERSION_MAJOR;

		if (a_skse->IsEditor()) {
			_FATALERROR("[FATAL ERROR] Loaded in editor, marking as incompatible!\n");
			return false;
		}

		switch (a_skse->RuntimeVersion()) {
		case RUNTIME_VERSION_1_5_73:
		case RUNTIME_VERSION_1_5_80:
			break;
		default:
			_FATALERROR("[FATAL ERROR] Unsupported runtime version %08X!\n", a_skse->RuntimeVersion());
			return false;
		}

		return true;
	}


	bool SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
	{
		_MESSAGE("[MESSAGE] YesImSure loaded");

		if (!SKSE::Init(a_skse)) {
			return false;
		}

		if (!Settings::loadSettings()) {
			_FATALERROR("[FATALERROR] Failed to load settings!\n");
			return false;
		}

#if _DEBUG
		Settings::dump();
#endif

		if (!g_localTrampoline.Create(1024 * 1)) {
			_FATALERROR("[FATALERROR] Failed to create local trampoline!\n");
			return false;
		}

		if (!g_branchTrampoline.Create(1024 * 1)) {
			_FATALERROR("[FATALERROR] Failed to create branch trampoline!\n");
			return false;
		}

		InstallHooks();
		_MESSAGE("[MESSAGE] Installed hooks");

		return true;
	}
};
