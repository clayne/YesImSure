#include "Hooks.h"

#include <cassert>
#include <cstdio>
#include <memory>

#include "Settings.h"

#include "RE/Skyrim.h"
#include "REL/Relocation.h"
#include "SKSE/API.h"
#include "SKSE/CodeGenerator.h"
#include "SKSE/SafeWrite.h"


namespace Hooks
{
	namespace
	{
		template <class T, std::uint64_t FUNC_ID>
		void SkipSubMenuMenuPrompt()
		{
			using func_t = void(T*);
			REL::Offset<func_t*> func = REL::ID(FUNC_ID);

			auto ui = RE::UI::GetSingleton();
			auto craftingMenu = ui->GetMenu<RE::CraftingMenu>();
			auto subMenu = static_cast<T*>(craftingMenu->subMenu);
			func(subMenu);
		}


		template <std::uint64_t FUNC_ID, std::size_t CAVE_START, std::size_t CAVE_END, std::size_t JUMP_OUT>
		void InstallSubMenuPatch(std::uintptr_t a_skipFuncAddr)
		{
			constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;
			constexpr UInt8 NOP = 0x90;

			REL::Offset<std::uintptr_t> funcBase = REL::ID(FUNC_ID);

			struct Patch : SKSE::CodeGenerator
			{
				Patch(std::size_t a_callAddr, std::size_t a_retAddr) : SKSE::CodeGenerator()
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

			Patch patch(a_skipFuncAddr, funcBase.GetAddress() + JUMP_OUT);
			patch.finalize();

			for (std::size_t i = 0; i < patch.getSize(); ++i) {
				SKSE::SafeWrite8(funcBase.GetAddress() + CAVE_START + i, patch.getCode()[i]);
			}

			for (std::size_t i = CAVE_START + patch.getSize(); i < CAVE_END; ++i) {
				SKSE::SafeWrite8(funcBase.GetAddress() + i, NOP);
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
			constexpr std::size_t JUMP_OUT = 0x148;
			constexpr UInt8 NOP = 0x90;

			REL::Offset<std::uintptr_t> funcBase = REL::ID(39406);

			// nop until callback gets loaded
			{
				constexpr std::size_t CAVE_START = 0xA3;
				constexpr std::size_t CAVE_END = 0xD7;

				for (std::size_t i = CAVE_START; i < CAVE_END; ++i) {
					SKSE::SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			// hook callback
			{
				constexpr std::size_t CAVE_START = 0xDE;
				constexpr std::size_t CAVE_END = 0x112;
				constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

				struct Patch : SKSE::CodeGenerator
				{
					Patch(std::size_t a_callAddr, std::size_t a_retAddr) : SKSE::CodeGenerator(CAVE_SIZE)
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

				Patch patch(unrestricted_cast<std::uintptr_t>(&RefreshInventoryMenu), funcBase.GetAddress() + JUMP_OUT);
				patch.finalize();

				for (std::size_t i = 0; i < patch.getSize(); ++i) {
					SKSE::SafeWrite8(funcBase.GetAddress() + CAVE_START + i, patch.getCode()[i]);
				}

				for (std::size_t i = CAVE_START + patch.getSize(); i < CAVE_END; ++i) {
					SKSE::SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			// swap messagebox error for debug notification
			{
				constexpr std::size_t CAVE_START = 0x119;
				constexpr std::size_t CAVE_END = 0x148;
				constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

				struct Patch : SKSE::CodeGenerator
				{
					Patch(std::size_t a_callAddr, std::size_t a_retAddr) : SKSE::CodeGenerator(CAVE_SIZE)
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

				REL::Offset<std::uintptr_t> dbgNotif(RE::Offset::DebugNotification);
				Patch patch(dbgNotif.GetAddress(), funcBase.GetAddress() + JUMP_OUT);
				patch.finalize();

				for (std::size_t i = 0; i < patch.getSize(); ++i) {
					SKSE::SafeWrite8(funcBase.GetAddress() + CAVE_START + i, patch.getCode()[i]);
				}

				for (std::size_t i = CAVE_START + patch.getSize(); i < CAVE_END; ++i) {
					SKSE::SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			// Fix for applying poison to left hand
			{
				auto trampoline = SKSE::GetTrampoline();
				trampoline->Write5Call(funcBase.GetAddress() + 0x2F, &GetEquippedEntryData);

				{
					REL::Offset<std::uintptr_t> funcBase = REL::ID(39407);
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
			auto msg = std::make_unique<char[]>(len);
			std::snprintf(msg.get(), len, a_fmt, name);
			RE::DebugNotification(msg.get());
		}


		void InstallEnchantmentLearnedPatch()
		{
			constexpr std::uint64_t SKIP_FUNC = 50459;
			constexpr std::size_t JUMP_OUT = 0x1EB;
			constexpr UInt8 NOP = 0x90;

			REL::Offset<std::uintptr_t> funcBase = REL::ID(SKIP_FUNC);

			// skip "are you sure?"
			{
				constexpr std::uint64_t CALL_FUNC = 50440;
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
					SKSE::SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			// swap messagebox for debug notification
			{
				constexpr std::size_t CAVE_START = 0x1AE;
				constexpr std::size_t CAVE_END = 0x1EB;
				constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

				struct Patch : SKSE::CodeGenerator
				{
					Patch(std::size_t a_callAddr, std::size_t a_retAddr) : SKSE::CodeGenerator(CAVE_SIZE)
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

				Patch patch(unrestricted_cast<std::uintptr_t>(NotifyEnchantmentLearned), funcBase.GetAddress() + JUMP_OUT);
				patch.finalize();

				for (std::size_t i = 0; i < patch.getSize(); ++i) {
					SKSE::SafeWrite8(funcBase.GetAddress() + CAVE_START + i, patch.getCode()[i]);
				}

				for (std::size_t i = CAVE_START + patch.getSize(); i < CAVE_END; ++i) {
					SKSE::SafeWrite8(funcBase.GetAddress() + i, NOP);
				}
			}

			_MESSAGE("Installled enchantment learned patch");
		}


		void CloseEnchantingMenu()
		{
			auto uiStr = RE::InterfaceStrings::GetSingleton();
			auto factory = RE::MessageDataFactoryManager::GetSingleton();
			auto creator = factory->GetCreator<RE::BSUIMessageData>(uiStr->bsUIMessageData);
			auto msg = creator->Create();
			msg->fixedStr = "Cancel";
			auto uiQueue = RE::UIMessageQueue::GetSingleton();
			uiQueue->AddMessage(uiStr->craftingMenu, RE::UI_MESSAGE_TYPE::kUserEvent, msg);
		}
	}


	void Install()
	{
		if (*Settings::constructibleObjectMenuPatch) {
			constexpr std::uint64_t CALL_FUNC = 50452;
			constexpr std::uint64_t SKIP_FUNC = 50476;
			constexpr std::size_t CAVE_START = 0x5F;
			constexpr std::size_t CAVE_END = 0x174;
			constexpr std::size_t JUMP_OUT = 0x192;
			auto fnAddr = unrestricted_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::ConstructibleObjectMenu, SKIP_FUNC>);
			InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			_MESSAGE("Installled constructible object menu patch");
		}

		if (*Settings::alchemyMenuPatch) {
			constexpr std::uint64_t CALL_FUNC = 50485;
			constexpr std::uint64_t SKIP_FUNC = 50447;
			constexpr std::size_t CAVE_START = 0x138;
			constexpr std::size_t CAVE_END = 0x2A9;
			constexpr std::size_t JUMP_OUT = 0x2AB;
			auto fnAddr = unrestricted_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::AlchemyMenu, SKIP_FUNC>);
			InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			_MESSAGE("Installled alchemy menu patch");
		}

		if (*Settings::smithingMenuPatch) {
			constexpr std::uint64_t CALL_FUNC = 50451;
			constexpr std::uint64_t SKIP_FUNC = 50477;
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
			constexpr std::uint64_t CALL_FUNC = 50487;

			if (*Settings::enchantmentCraftedPatch) {
				constexpr std::uint64_t SKIP_FUNC = 50450;
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
