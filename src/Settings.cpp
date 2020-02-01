#include "settings.h"

#include "Json2Settings.h"


bool Settings::LoadSettings(bool a_dumpParse)
{
	auto [log, success] = Json2Settings::load_settings(FILE_NAME, a_dumpParse);
	if (!log.empty()) {
		_ERROR("%s", log.c_str());
	}
	return success;
}


decltype(Settings::constructibleObjectMenuPatch) Settings::constructibleObjectMenuPatch("constructibleObjectMenuPatch", true);
decltype(Settings::alchemyMenuPatch) Settings::alchemyMenuPatch("alchemyMenuPatch", true);
decltype(Settings::smithingMenuPatch) Settings::smithingMenuPatch("smithingMenuPatch", true);
decltype(Settings::enchantmentLearnedPatch) Settings::enchantmentLearnedPatch("enchantmentLearnedPatch", false);
decltype(Settings::enchantmentCraftedPatch) Settings::enchantmentCraftedPatch("enchantmentCraftedPatch", false);
decltype(Settings::enchantingMenuExitPatch) Settings::enchantingMenuExitPatch("enchantingMenuExitPatch", true);
decltype(Settings::poisonPatch) Settings::poisonPatch("poisonPatch", true);
