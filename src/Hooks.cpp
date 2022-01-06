#include "Hooks.h"

#include "Settings.h"

namespace Hooks
{
	namespace
	{
		template <class T, std::uint64_t FUNC_ID>
		void SkipSubMenuMenuPrompt()
		{
			REL::Offset<void(T*)> func = REL::ID(FUNC_ID);
			const auto ui = RE::UI::GetSingleton();
			const auto craftingMenu = ui->GetMenu<RE::CraftingMenu>();
			const auto subMenu = static_cast<T*>(craftingMenu->subMenu);
			func(subMenu);
		}

		struct SubMenuPatchCode :
			public Xbyak::CodeGenerator
		{
		public:
			SubMenuPatchCode(std::size_t a_callAddr, std::size_t a_retAddr)
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

		template <std::uint64_t FUNC_ID, std::size_t CAVE_START, std::size_t CAVE_END, std::size_t JUMP_OUT>
		void InstallSubMenuPatch(std::uintptr_t a_skipFuncAddr)
		{
			constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

			const REL::Relocation<std::uintptr_t> target{ REL::ID(FUNC_ID), CAVE_START };

			SubMenuPatchCode patch(a_skipFuncAddr, funcBase.GetAddress() + JUMP_OUT);
			patch.ready();
			assert(patch.getSize() <= CAVE_SIZE);

			REL::safe_fill(target.address(), REL::NOP, CAVE_SIZE);
			REL::safe_write(
				target.address(),
				std::span{ patch.getCode<const std::byte*>(), patch.getSize() });
		}

		void RefreshInventoryMenu()
		{
			const auto ui = RE::UI::GetSingleton();
			const auto invMenu = ui->GetMenu<RE::InventoryMenu>();
			if (invMenu && invMenu->itemList) {
				invMenu->itemList->Update();
			}
		}

		RE::InventoryEntryData* GetEquippedEntryData(RE::AIProcess* a_process, [[maybe_unused]] bool a_leftHand)
		{
			const auto middleHigh = a_process->middleHigh;
			if (!middleHigh) {
				return nullptr;
			}

			const auto hand = middleHigh->rightHand;
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

			const REL::Relocation<std::uintptr_t> funcBase{ REL::ID(40481) };

			// nop until callback gets loaded
			{
				constexpr std::size_t CAVE_START = 0xA3;
				constexpr std::size_t CAVE_END = 0xD7;
				REL::safe_fill(funcBase.address() + CAVE_START, REL::NOP, CAVE_END - CAVE_START);
			}

			// hook callback
			{
				constexpr std::size_t CAVE_START = 0xDE;
				constexpr std::size_t CAVE_END = 0x112;
				constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

				struct Patch :
					public Xbyak::CodeGenerator
				{
				public:
					Patch(std::size_t a_callAddr, std::size_t a_retAddr)
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

				Patch patch(reinterpret_cast<std::uintptr_t>(&RefreshInventoryMenu), funcBase.address() + JUMP_OUT);
				patch.ready();
				assert(patch.getSize() <= CAVE_SIZE);

				REL::safe_fill(funcBase.address() + CAVE_START, REL::NOP, CAVE_SIZE);
				REL::safe_write(
					funcBase.address() + CAVE_START,
					std::span{ patch.getCode<const std::byte*>(), patch.getSize() });
			}

			// swap messagebox error for debug notification
			{
				constexpr std::size_t CAVE_START = 0x119;
				constexpr std::size_t CAVE_END = 0x148;
				constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

				struct Patch :
					public Xbyak::CodeGenerator
				{
				public:
					Patch(std::size_t a_callAddr, std::size_t a_retAddr)
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

				const REL::Relocation<std::uintptr_t> dbgNotif{ REL::ID(52933) };
				Patch patch(dbgNotif.address(), funcBase.address() + JUMP_OUT);
				patch.ready();
				assert(patch.getSize() <= CAVE_SIZE);

				REL::safe_fill(funcBase.address() + CAVE_START, REL::NOP, CAVE_SIZE);
				REL::safe_write(
					funcBase.address() + CAVE_START,
					std::span{ patch.getCode<const std::byte*>(), patch.getSize() });
			}

			// Fix for applying poison to left hand
			{
				auto& trampoline = SKSE::GetTrampoline();
				trampoline.write_call<5>(funcBase.address() + 0x2F, &GetEquippedEntryData);

				const REL::Relocation<std::uintptr_t> funcBase2{ REL::ID(40482) };
				trampoline.write_call<5>(funcBase2.address() + 0x32, &GetEquippedEntryData);
			}

			logger::debug("Installled poison patch"sv);
		}

		void NotifyEnchantmentLearned(const char* a_fmt, RE::TESForm* a_item)
		{
			const auto fullName = a_item->As<RE::TESFullName>();
			const auto name = fullName ? fullName->GetFullName() : "";
			std::size_t len = std::snprintf(0, 0, a_fmt, name) + 1;
			const auto msg = std::make_unique<char[]>(len);
			std::snprintf(msg.get(), len, a_fmt, name);
			RE::DebugNotification(msg.get());
		}

		void InstallEnchantmentLearnedPatch()
		{
			constexpr std::uint64_t SKIP_FUNC = 51363;

			const REL::Relocation<std::uintptr_t> funcBase{ REL::ID(SKIP_FUNC) };

			// skip "are you sure?"
			{
				constexpr std::uint64_t CALL_FUNC = 51344;
				constexpr std::size_t CAVE_START = 0xC2;
				constexpr std::size_t CAVE_END = 0x1D7;
				constexpr std::size_t JUMP_OUT = 0x4DF;
				const auto fnAddr = reinterpret_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::EnchantConstructMenu, SKIP_FUNC>);
				InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			}

			// nop until format string gets loaded
			{
				constexpr std::size_t CAVE_START = 0x15D;
				constexpr std::size_t CAVE_END = 0x1A7;
				REL::safe_fill(funcBase.address() + CAVE_START, REL::NOP, CAVE_END - CAVE_START);
			}

			// swap messagebox for debug notification
			{
				constexpr std::size_t CAVE_START = 0x1AE;
				constexpr std::size_t CAVE_END = 0x1EB;
				constexpr std::size_t CAVE_SIZE = CAVE_END - CAVE_START;

				struct Patch :
					public Xbyak::CodeGenerator
				{
				public:
					Patch(std::size_t a_callAddr, std::size_t a_retAddr)
					{
						Xbyak::Label callLbl;
						Xbyak::Label retLbl;

						mov(rcx, rbx);  // rbx == const char*
						mov(rdx, rsi);  // rsi == TESForm*
						call(ptr[rip + callLbl]);
						jmp(ptr[rip + retLbl]);

						L(callLbl);
						dq(a_callAddr);

						L(retLbl);
						dq(a_retAddr);
					}
				};

				constexpr std::size_t JUMP_OUT = 0x1EB;
				Patch patch(reinterpret_cast<std::uintptr_t>(NotifyEnchantmentLearned), funcBase.address() + JUMP_OUT);
				patch.ready();
				assert(patch.getSize() <= CAVE_SIZE);

				REL::safe_fill(funcBase.address() + CAVE_START, REL::NOP, CAVE_SIZE);
				REL::safe_write(
					funcBase.address() + CAVE_START,
					std::span{ patch.getCode<const std::byte*>(), patch.getSize() });
			}

			logger::debug("Installled enchantment learned patch"sv);
		}

		void CloseEnchantingMenu()
		{
			const auto uiStr = RE::InterfaceStrings::GetSingleton();
			const auto factory = RE::MessageDataFactoryManager::GetSingleton();
			const auto creator = factory->GetCreator<RE::BSUIMessageData>(uiStr->bsUIMessageData);
			const auto msg = creator->Create();
			msg->fixedStr = "Cancel";
			const auto uiQueue = RE::UIMessageQueue::GetSingleton();
			uiQueue->AddMessage(uiStr->craftingMenu, RE::UI_MESSAGE_TYPE::kUserEvent, msg);
		}
	}

	void Install()
	{
		if (*Settings::ConstructibleObjectMenu) {
			constexpr std::uint64_t CALL_FUNC = 51357;
			constexpr std::uint64_t SKIP_FUNC = 51369;
			constexpr std::size_t CAVE_START = 0x5A;
			constexpr std::size_t CAVE_END = 0x169;
			constexpr std::size_t JUMP_OUT = 0x187;
			const auto fnAddr = reinterpret_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::ConstructibleObjectMenu, SKIP_FUNC>);
			InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			logger::debug("Installled constructible object menu patch"sv);
		}

		if (*Settings::AlchemyMenu) {
			constexpr std::uint64_t CALL_FUNC = 51377;
			constexpr std::uint64_t SKIP_FUNC = 51352;
			constexpr std::size_t CAVE_START = 0x14B;
			constexpr std::size_t CAVE_END = 0x2A0;
			constexpr std::size_t JUMP_OUT = 0x2A0;
			const auto fnAddr = reinterpret_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::AlchemyMenu, SKIP_FUNC>);
			InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			logger::debug("Installled alchemy menu patch"sv);
		}

		if (*Settings::SmithingMenu) {
			constexpr std::uint64_t CALL_FUNC = 51356;
			constexpr std::uint64_t SKIP_FUNC = 51370;
			constexpr std::size_t CAVE_START = 0x7D;
			constexpr std::size_t CAVE_END = 0x191;
			constexpr std::size_t JUMP_OUT = 0x1E4;
			const auto fnAddr = reinterpret_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::SmithingMenu, SKIP_FUNC>);
			InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
			logger::debug("Installled smithing menu patch"sv);
		}

		if (*Settings::EnchantmentLearned) {
			InstallEnchantmentLearnedPatch();
		}

		{
			constexpr std::uint64_t CALL_FUNC = 51379;

			if (*Settings::EnchantmentCrafted) {
				constexpr std::uint64_t SKIP_FUNC = 51355;
				constexpr std::size_t CAVE_START = 0x185;
				constexpr std::size_t CAVE_END = 0x23F;
				constexpr std::size_t JUMP_OUT = 0x2C2;
				const auto fnAddr = reinterpret_cast<std::uintptr_t>(SkipSubMenuMenuPrompt<RE::CraftingSubMenus::EnchantConstructMenu, SKIP_FUNC>);
				InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
				logger::debug("Installled enchantment crafted patch"sv);
			}

			if (*Settings::EnchantingMenuExit) {
				constexpr std::size_t CAVE_START = 0x6F;
				constexpr std::size_t CAVE_END = 0x129;
				constexpr std::size_t JUMP_OUT = 0x129;
				const auto fnAddr = reinterpret_cast<std::uintptr_t>(CloseEnchantingMenu);
				InstallSubMenuPatch<CALL_FUNC, CAVE_START, CAVE_END, JUMP_OUT>(fnAddr);
				logger::debug("Installled enchanting menu exit patch"sv);
			}
		}

		if (*Settings::Poison) {
			InstallPoisonPatch();
		}

		logger::debug("Installed hooks"sv);
	}
}
