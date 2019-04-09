#include "Hooks.h"

#include "skse64_common/BranchTrampoline.h"  // g_localTrampoline
#include "skse64_common/SafeWrite.h"  // SafeWrite8
#include "skse64_common/Utilities.h"  // GetFnAddr
#include "skse64/GameRTTI.h"  // Runtime_DynamicCast
#include "xbyak/xbyak.h"  // xbyak

#include <cassert>  // assert

#include "RE/Skyrim.h"


namespace
{
	template <class T, std::uintptr_t FUNC_ADDR>
	void SkipSubMenuMenuPrompt()
	{
		using func_t = void(T*);
		RelocUnrestricted<func_t*> func(FUNC_ADDR);

		auto mm = RE::MenuManager::GetSingleton();
		auto uiStrHolder = RE::UIStringHolder::GetSingleton();
		auto craftingMenu = mm->GetMenu<RE::CraftingMenu>(uiStrHolder->craftingMenu);
		auto subMenu = skyrim_cast<T*>(craftingMenu->subMenu);
		if (subMenu) {
			func(subMenu);
		}
	}


	template <class T, std::uintptr_t CALL_FUNC, std::uintptr_t SKIP_FUNC, std::size_t CAVE_START, std::size_t CAVE_END, std::size_t JUMP_OUT>
	void InstallSubMenuPatch()
	{
		constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;
		constexpr UInt8 NOP = 0x90;

		RelocUnrestricted<std::uintptr_t> funcBase(CALL_FUNC);

		struct Patch : Xbyak::CodeGenerator
		{
			Patch(void* a_buf, std::size_t a_callAddr, std::size_t a_retAddr) : Xbyak::CodeGenerator(1024, a_buf)
			{
				Xbyak::Label callLbl;
				Xbyak::Label retLbl;

				call(ptr[rip + callLbl]);
				jmp(ptr[rip + retLbl]);

				L(callLbl);
				dq(a_callAddr);

				L(retLbl);
				dq(a_retAddr);
			}
		};

		void* patchBuf = g_localTrampoline.StartAlloc();
		Patch patch(patchBuf, GetFnAddr(&SkipSubMenuMenuPrompt<T, SKIP_FUNC>), funcBase.GetUIntPtr() + JUMP_OUT);
		g_localTrampoline.EndAlloc(patch.getCurr());

		assert(patch.getSize() <= CAVE_SIZE);

		for (std::size_t i = 0; i < patch.getSize(); ++i) {
			SafeWrite8(funcBase.GetUIntPtr() + CAVE_START + i, patch.getCode()[i]);
		}

		for (std::size_t i = CAVE_START + patch.getSize(); i < CAVE_END; ++i) {
			SafeWrite8(funcBase.GetUIntPtr() + i, NOP);
		}
	}
}


void InstallHooks()
{
	{
		// 40 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 48 48 89 6C 24 50 48 89 74 24 58 83 79 30 00
		constexpr std::uintptr_t CALL_FUNC = 0x0086CC10;	// 1_5_73
		// 40 53 55 56 57 41 56 48 83 EC 60 48 C7 44 24 30 FE FF FF FF
		constexpr std::uintptr_t SKIP_FUNC = 0x0086E2C0;	// 1_5_73
		constexpr std::size_t CAVE_START = 0x5F;
		constexpr std::size_t CAVE_END = 0x174;
		constexpr std::size_t JUMP_OUT = 0x192;
		InstallSubMenuPatch<RE::CraftingSubMenus::ConstructibleObjectMenu, CALL_FUNC, SKIP_FUNC, CAVE_START, CAVE_END, JUMP_OUT>();
		_MESSAGE("[MESSAGE] Installled ConstructibleObjectMenu Patch");
	}

	{
		// 48 8B C4 55 56 57 48 83 EC 60 48 C7 40 C8 FE FF FF FF 48 89 58 08
		constexpr std::uintptr_t CALL_FUNC = 0x0086EBB0;	// 1_5_73
		// E8 ? ? ? ? E9 ? ? ? ? 48 85 C9 0F 84 ? ? ? ? 48 8B 01
		constexpr std::uintptr_t SKIP_FUNC = 0x0086B810;	// 1_5_73
		constexpr std::size_t CAVE_START = 0x138;
		constexpr std::size_t CAVE_END = 0x2A9;
		constexpr std::size_t JUMP_OUT = 0x2AB;
		InstallSubMenuPatch<RE::CraftingSubMenus::AlchemyMenu, CALL_FUNC, SKIP_FUNC, CAVE_START, CAVE_END, JUMP_OUT>();
		_MESSAGE("[MESSAGE] Installled AlchemyMenu Patch");
	}


	{
		// 40 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 48 48 89 6C 24 50 48 89 74 24 58 48 8B E9 8B 81 54 01 00 00
		constexpr std::uintptr_t CALL_FUNC = 0x0086CA10;	// 1_5_73
		// 48 8B C4 56 57 41 56 48 83 EC 70 48 C7 40 B8 FE FF FF FF 48 89 58 18 48 89 68 20 48 8B F1
		constexpr std::uintptr_t SKIP_FUNC = 0x0086E490;	// 1_5_73
		constexpr std::size_t CAVE_START = 0x7C;
		constexpr std::size_t CAVE_END = 0x191;
		constexpr std::size_t JUMP_OUT = 0x1E5;
		InstallSubMenuPatch<RE::CraftingSubMenus::SmithingMenu, CALL_FUNC, SKIP_FUNC, CAVE_START, CAVE_END, JUMP_OUT>();
		_MESSAGE("[MESSAGE] Installled SmithingMenu Patch");
	}
}
