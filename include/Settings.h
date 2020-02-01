#pragma once

#include "Json2Settings.h"


class Settings
{
public:
	using bSetting = Json2Settings::bSetting;


	Settings() = delete;

	static bool LoadSettings(bool a_dumpParse = false);


	static bSetting	constructibleObjectMenuPatch;
	static bSetting	alchemyMenuPatch;
	static bSetting	smithingMenuPatch;
	static bSetting	enchantmentLearnedPatch;
	static bSetting	enchantmentCraftedPatch;
	static bSetting	enchantingMenuExitPatch;
	static bSetting	poisonPatch;

private:
	static inline constexpr char FILE_NAME[] = "Data\\SKSE\\Plugins\\YesImSure.json";
};
