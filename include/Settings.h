#pragma once

#include "Json2Settings.h"  // Json2Settings


class Settings : public Json2Settings::Settings
{
public:
	Settings() = delete;
	static bool loadSettings(bool a_dumpParse = false);


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
