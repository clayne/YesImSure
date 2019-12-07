#include "skse64_common/BranchTrampoline.h"
#include "skse64_common/skse_version.h"

#include "Hooks.h"
#include "Settings.h"
#include "version.h"

#include "SKSE/API.h"


extern "C" {
	bool SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
	{
		SKSE::Logger::OpenRelative(FOLDERID_Documents, L"\\My Games\\Skyrim Special Edition\\SKSE\\YesImSure.log");
		SKSE::Logger::SetPrintLevel(SKSE::Logger::Level::kDebugMessage);
		SKSE::Logger::SetFlushLevel(SKSE::Logger::Level::kDebugMessage);
		SKSE::Logger::UseLogStamp(true);

		_MESSAGE("YesImSure v%s", YISR_VERSION_VERSTRING);

		a_info->infoVersion = SKSE::PluginInfo::kVersion;
		a_info->name = "YesImSure";
		a_info->version = YISR_VERSION_MAJOR;

		if (a_skse->IsEditor()) {
			_FATALERROR("Loaded in editor, marking as incompatible!\n");
			return false;
		}

		switch (a_skse->RuntimeVersion()) {
		case RUNTIME_VERSION_1_5_97:
			break;
		default:
			_FATALERROR("Unsupported runtime version %s!\n", a_skse->UnmangledRuntimeVersion());
			return false;
		}

		return true;
	}


	bool SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
	{
		_MESSAGE("YesImSure loaded");

		if (!SKSE::Init(a_skse)) {
			return false;
		}

		if (!Settings::loadSettings()) {
			_FATALERROR("Failed to load settings!\n");
			return false;
		}

#if _DEBUG
		Settings::dump();
#endif

		if (!g_localTrampoline.Create(1024 * 1)) {
			_FATALERROR("Failed to create local trampoline!\n");
			return false;
		}

		if (!g_branchTrampoline.Create(1024 * 1)) {
			_FATALERROR("Failed to create branch trampoline!\n");
			return false;
		}

		InstallHooks();
		_MESSAGE("Installed hooks");

		return true;
	}
};
