#include "Hooks.h"

#include "skse64_common/SafeWrite.h"
#include "xbyak/xbyak.h"

#include <cassert>
#include <cstdio>
#include <memory>

#include "Settings.h"

#include "RE/Skyrim.h"
#include "REL/Relocation.h"
#include "SKSE/API.h"


namespace Hooks
{
	namespace
	{
		template <class T, std::uintptr_t FUNC_ADDR>
		void SkipSubMenuMenuPrompt()
		{
			using func_t = void(T*);
			REL::Offset<func_t*> func(FUNC_ADDR);

			auto ui = RE::UI::GetSingleton();
			auto craftingMenu = ui->GetMenu<RE::CraftingMenu>();
			auto subMenu = static_cast<T*>(craftingMenu->subMenu);
			func(subMenu);
		}


		template <std::uintptr_t FUNC_ADDR, std::size_t CAVE_START, std::size_t CAVE_END, std::size_t JUMP_OUT>
		void InstallSubMenuPatch(std::uintptr_t a_skipFuncAddr)
		{
			constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;
			constexpr UInt8 NOP = 0x90;

			REL::Offset<std::uintptr_t> funcBase(FUNC_ADDR);

			struct Patch : Xbyak::CodeGenerator
			{
				Patch(std::size_t a_maxSize, void* a_buf, std::size_t a_callAddr, std::size_t a_retAddr) : Xbyak::CodeGenerator(a_maxSize, a_buf)
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

			auto trampoline = SKSE::GetTrampoline();
			auto patchBuf = trampoline->StartAlloc();
			Patch patch(trampoline->FreeSize(), patchBuf, a_skipFuncAddr, funcBase.GetAddress() + JUMP_OUT);
			trampoline->EndAlloc(patch.getSize());

			assert(patch.getSize() <= CAVE_SIZE);

			for (std::size_t i = 0; i < patch.getSize(); ++i) {
				SafeWrite8(funcBase.GetAddress() + CAVE_START + i, patch.getCode()[i]);
			}

			for (std::size_t i = CAVE_START + patch.getSize(); i < CAVE_END; ++i) {
				SafeWrite8(funcBase.GetAddress() + i, NOP);
			}
		}


		void RefreshInventoryMenu()
		{
			auto ui = RE::UI::GetSingleton();
			auto invMenu = ui->GetMenu<RE::InventoryMenu>();
			if (invMenu && invMenu->itemList) {
				invMenu->itemList->Update();
			}
		}


		RE::InventoryEntryData* GetEquippedEntryData(RE::AIProcess* a_process, bool a_leftHand)
		{
			auto middleHigh = a_process->middleHigh;
			if (!middleHigh) {
				return 0;
			}

			auto hand = middleHigh->rightHand;
			if (hand && hand->object && hand->object->Is(RE::FormType::Weapon)) {
				if (!hand->extraLists) {
					return hand;
				}

				bool applied = false;
				for (auto& xList : *hand->extraLists) {
					if (xList->HasType(RE::ExtraDataType::kPoison)) {
						applied = true;
						break;
					}
				}

				if (!applied) {
					return hand;
				}
			}

			return middleHigh->leftHand ? middleHigh->leftHand : middleHigh->rightHand;
		}


		void InstallPoisonPatch()
		{
			// E8 ? ? ? ? E9 ? ? ? ? 48 8D 55 CC
			constexpr std::uintptr_t FUNC_ADDR = 0x006A1CC0;	// 1_5_97
			constexpr std::size_t JUMP_OUT = 0x148;
			constexpr UInt8 NOP = 0x90;

			REL::Offset<std::uintptr_t> funcBase(FUNC_ADDR);

			// nop until callback gets loaded
			{
				constexpr std::size_t CAVE_START = 0xA3;
				constexpr std::size_t CAVE_END = 0xD7;

				for (std::size_t i = CAVE_START; i < CAVE_END; ++i) {
					SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			// hook callback
			{
				constexpr std::size_t CAVE_START = 0xDE;
				constexpr std::size_t CAVE_END = 0x112;
				constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(std::size_t a_maxSize, void* a_buf, std::size_t a_callAddr, std::size_t a_retAddr) : Xbyak::CodeGenerator(a_maxSize, a_buf)
					{
						Xbyak::Label callLbl;
						Xbyak::Label retLbl;

						mov(rcx, 2);
						call(rdx);
						call(ptr[rip + callLbl]);
						jmp(ptr[rip + retLbl]);

						L(callLbl);
						dq(a_callAddr);

						L(retLbl);
						dq(a_retAddr);
					}
				};

				auto trampoline = SKSE::GetTrampoline();
				auto patchBuf = trampoline->StartAlloc();
				Patch patch(trampoline->FreeSize(), patchBuf, unrestricted_cast<std::uintptr_t>(&RefreshInventoryMenu), funcBase.GetAddress() + JUMP_OUT);
				trampoline->EndAlloc(patch.getSize());

				assert(patch.getSize() <= CAVE_SIZE);

				for (std::size_t i = 0; i < patch.getSize(); ++i) {
					SafeWrite8(funcBase.GetAddress() + CAVE_START + i, patch.getCode()[i]);
				}

				for (std::size_t i = CAVE_START + patch.getSize(); i < CAVE_END; ++i) {
					SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			// swap messagebox error for debug notification
			{
				constexpr std::size_t CAVE_START = 0x119;
				constexpr std::size_t CAVE_END = 0x148;
				constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(std::size_t a_maxSize, void* a_buf, std::size_t a_callAddr, std::size_t a_retAddr) : Xbyak::CodeGenerator(a_maxSize, a_buf)
					{
						Xbyak::Label callLbl;
						Xbyak::Label retLbl;

						mov(rdx, 0);
						mov(r8, 1);
						call(ptr[rip + callLbl]);
						jmp(ptr[rip + retLbl]);

						L(callLbl);
						dq(a_callAddr);

						L(retLbl);
						dq(a_retAddr);
					}
				};

				auto trampoline = SKSE::GetTrampoline();
				auto patchBuf = trampoline->StartAlloc();
				REL::Offset<std::uintptr_t> dbgNotif(RE::Offset::DebugNotification);
				Patch patch(trampoline->FreeSize(), patchBuf, dbgNotif.GetAddress(), funcBase.GetAddress() + JUMP_OUT);
				trampoline->EndAlloc(patch.getSize());

				assert(patch.getSize() <= CAVE_SIZE);

				for (std::size_t i = 0; i < patch.getSize(); ++i) {
					SafeWrite8(funcBase.GetAddress() + CAVE_START + i, patch.getCode()[i]);
				}

				for (std::size_t i = CAVE_START + patch.getSize(); i < CAVE_END; ++i) {
					SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			// Fix for applying poison to left hand
			{
				auto trampoline = SKSE::GetTrampoline();
				trampoline->Write5Call(funcBase.GetAddress() + 0x2F, &GetEquippedEntryData);

				{
					// 40 53 48 83 EC 60 48 8B 05 ? ? ? ? 48 83 B8 68 09 00 00 00
					constexpr std::uintptr_t FUNC_ADDR = 0x006A1E30;	// 1_5_97
					REL::Offset<std::uintptr_t> funcBase(FUNC_ADDR);
					trampoline->Write5Call(funcBase.GetAddress() + 0x32, &GetEquippedEntryData);
				}
			}

			_MESSAGE("Installled poison patch");
		}


		void NotifyEnchantmentLearned(const char* a_fmt, RE::TESForm* a_item)
		{
			auto fullName = a_item->As<RE::TESFullName>();
			auto name = fullName ? fullName->GetFullName() : "";
			std::size_t len = std::snprintf(0, 0, a_fmt, name) + 1;
			std::unique_ptr<char[]> msg(new char[len]);
			std::snprintf(msg.get(), len, a_fmt, name);
			RE::DebugNotification(msg.get());
		}


		void InstallEnchantmentLearnedPatch()
		{
			// E9 ? ? ? ? 48 8B 41 10 8B 88 08 02 00 00
			constexpr std::uintptr_t SKIP_FUNC = 0x0086D830;	// 1_5_97
			constexpr std::size_t JUMP_OUT = 0x1EB;
			constexpr UInt8 NOP = 0x90;

			REL::Offset<std::uintptr_t> funcBase(SKIP_FUNC);

			// skip "are you sure?"
			{
				// 40 57 41 54 41 55 41 56 41 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 68 48 89 6C 24 70 48 89 74 24 78 8B EA
				constexpr std::uintptr_t CALL_FUNC = 0x0086B200;	// 1_5_97
				constexpr std::size_t CAVE_START = 0xBB;
				constexpr std::size_t CAVE_END = 0x1D3;
				constexpr std::size_t JUMP_OUT = 0x38D;
				auto fnAddr = unrestricted_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::EnchantConstructMenu, SKIP_FUNC>);
				InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			}

			// nop until format string gets loaded
			{
				constexpr std::size_t CAVE_START = 0x15D;
				constexpr std::size_t CAVE_END = 0x1A7;

				for (std::size_t i = CAVE_START; i < CAVE_END; ++i) {
					SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			// swap messagebox for debug notification
			{
				constexpr std::size_t CAVE_START = 0x1AE;
				constexpr std::size_t CAVE_END = 0x1EB;
				constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

				struct Patch : Xbyak::CodeGenerator
				{
					Patch(std::size_t a_maxSize, void* a_buf, std::size_t a_callAddr, std::size_t a_retAddr) : Xbyak::CodeGenerator(a_maxSize, a_buf)
					{
						Xbyak::Label callLbl;
						Xbyak::Label retLbl;

						mov(rcx, rbx);	// rbx == const char*
						mov(rdx, rsi);	// rsi == TESForm*
						call(ptr[rip + callLbl]);
						jmp(ptr[rip + retLbl]);

						L(callLbl);
						dq(a_callAddr);

						L(retLbl);
						dq(a_retAddr);
					}
				};

				auto trampoline = SKSE::GetTrampoline();
				auto patchBuf = trampoline->StartAlloc();
				Patch patch(trampoline->FreeSize(), patchBuf, unrestricted_cast<std::uintptr_t>(NotifyEnchantmentLearned), funcBase.GetAddress() + JUMP_OUT);
				trampoline->EndAlloc(patch.getCurr());

				assert(patch.getSize() <= CAVE_SIZE);

				for (std::size_t i = 0; i < patch.getSize(); ++i) {
					SafeWrite8(funcBase.GetAddress() + CAVE_START + i, patch.getCode()[i]);
				}

				for (std::size_t i = CAVE_START + patch.getSize(); i < CAVE_END; ++i) {
					SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			_MESSAGE("Installled enchantment learned patch");
		}


		void CloseEnchantingMenu()
		{
			using Message = RE::UI_MESSAGE_TYPE;

			auto uiStr = RE::InterfaceStrings::GetSingleton();
			auto factory = RE::MessageDataFactoryManager::GetSingleton();
			auto creator = factory->GetCreator<RE::BSUIMessageData>(uiStr->bsUIMessageData);
			auto msg = creator->Create();
			msg->fixedStr = "Cancel";
			auto uiQueue = RE::UIMessageQueue::GetSingleton();
			uiQueue->AddMessage(uiStr->craftingMenu, Message::kUserEvent, msg);
		}
	}


	void Install()
	{
		if (*Settings::constructibleObjectMenuPatch) {
			// 40 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 48 48 89 6C 24 50 48 89 74 24 58 83 79 30 00
			constexpr std::uintptr_t CALL_FUNC = 0x0086CC10;	// 1_5_97
			// 40 53 55 56 57 41 56 48 83 EC 60 48 C7 44 24 30 FE FF FF FF
			constexpr std::uintptr_t SKIP_FUNC = 0x0086E2C0;	// 1_5_97
			constexpr std::size_t CAVE_START = 0x5F;
			constexpr std::size_t CAVE_END = 0x174;
			constexpr std::size_t JUMP_OUT = 0x192;
			auto fnAddr = unrestricted_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::ConstructibleObjectMenu, SKIP_FUNC>);
			InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			_MESSAGE("Installled constructible object menu patch");
		}

		if (*Settings::alchemyMenuPatch) {
			// 48 8B C4 55 56 57 48 83 EC 60 48 C7 40 C8 FE FF FF FF 48 89 58 08
			constexpr std::uintptr_t CALL_FUNC = 0x0086EBB0;	// 1_5_97
			// E8 ? ? ? ? E9 ? ? ? ? 48 85 C9 0F 84 ? ? ? ? 48 8B 01
			constexpr std::uintptr_t SKIP_FUNC = 0x0086B810;	// 1_5_97
			constexpr std::size_t CAVE_START = 0x138;
			constexpr std::size_t CAVE_END = 0x2A9;
			constexpr std::size_t JUMP_OUT = 0x2AB;
			auto fnAddr = unrestricted_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::AlchemyMenu, SKIP_FUNC>);
			InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			_MESSAGE("Installled alchemy menu patch");
		}

		if (*Settings::smithingMenuPatch) {
			// 40 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 48 48 89 6C 24 50 48 89 74 24 58 48 8B E9 8B 81 54 01 00 00
			constexpr std::uintptr_t CALL_FUNC = 0x0086CA10;	// 1_5_97
			// 48 8B C4 56 57 41 56 48 83 EC 70 48 C7 40 B8 FE FF FF FF 48 89 58 18 48 89 68 20 48 8B F1
			constexpr std::uintptr_t SKIP_FUNC = 0x0086E490;	// 1_5_97
			constexpr std::size_t CAVE_START = 0x7C;
			constexpr std::size_t CAVE_END = 0x191;
			constexpr std::size_t JUMP_OUT = 0x1E5;
			auto fnAddr = unrestricted_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::SmithingMenu, SKIP_FUNC>);
			InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			_MESSAGE("Installled smithing menu patch");
		}

		if (*Settings::enchantmentLearnedPatch) {
			InstallEnchantmentLearnedPatch();
		}

		{
			// 40 57 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 48 48 89 6C 24 50 48 89 74 24 58 48 8B F9 0F B6 81 1D 02 00 00
			constexpr std::uintptr_t CALL_FUNC = 0x0086EE80;	// 1_5_97

			if (*Settings::enchantmentCraftedPatch) {
				// 40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 80 00 00 00 48 C7 44 24 50 FE FF FF FF
				constexpr std::uintptr_t SKIP_FUNC = 0x0086C640;	// 1_5_97
				constexpr std::size_t CAVE_START = 0x160;
				constexpr std::size_t CAVE_END = 0x1FD;
				constexpr std::size_t JUMP_OUT = 0x260;
				auto fnAddr = unrestricted_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::EnchantConstructMenu, SKIP_FUNC>);
				InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
				_MESSAGE("Installled enchantment crafted patch");
			}

			if (*Settings::enchantingMenuExitPatch) {
				constexpr std::size_t CAVE_START = 0x74;
				constexpr std::size_t CAVE_END = 0x111;
				constexpr std::size_t JUMP_OUT = 0x111;
				auto fnAddr = unrestricted_cast<std::uintptr_t>(CloseEnchantingMenu);
				InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
				_MESSAGE("Installled enchanting menu exit patch");
			}
		}

		if (*Settings::poisonPatch) {
			InstallPoisonPatch();
		}

		_MESSAGE("Installed hooks");
	}
}
