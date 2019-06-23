#include "settings.h"

#include "Json2Settings.h"  // Json2Settings


bool Settings::loadSettings(bool a_dumpParse)
{
	return Json2Settings::Settings::loadSettings(FILE_NAME, false, a_dumpParse);
}


decltype(Settings::constructibleObjectMenuPatch)	Settings::constructibleObjectMenuPatch("constructibleObjectMenuPatch", false, true);
decltype(Settings::alchemyMenuPatch)				Settings::alchemyMenuPatch("alchemyMenuPatch", false, true);
decltype(Settings::smithingMenuPatch)				Settings::smithingMenuPatch("smithingMenuPatch", false, true);
decltype(Settings::enchantmentLearnedPatch)			Settings::enchantmentLearnedPatch("enchantmentLearnedPatch", false, false);
decltype(Settings::enchantmentCraftedPatch)			Settings::enchantmentCraftedPatch("enchantmentCraftedPatch", false, false);
decltype(Settings::enchantingMenuExitPatch)			Settings::enchantingMenuExitPatch("enchantingMenuExitPatch", false, true);
decltype(Settings::poisonPatch)						Settings::poisonPatch("poisonPatch", false, true);
