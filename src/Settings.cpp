#include "settings.h"

#include "Json2Settings.h"  // Json2Settings


bool Settings::loadSettings(bool a_dumpParse)
{
	return Json2Settings::Settings::loadSettings(FILE_NAME, a_dumpParse);
}


decltype(Settings::constructibleObjectMenuPatch)	Settings::constructibleObjectMenuPatch("constructibleObjectMenuPatch", true);
decltype(Settings::alchemyMenuPatch)				Settings::alchemyMenuPatch("alchemyMenuPatch", true);
decltype(Settings::smithingMenuPatch)				Settings::smithingMenuPatch("smithingMenuPatch", true);
decltype(Settings::enchantmentLearnedPatch)			Settings::enchantmentLearnedPatch("enchantmentLearnedPatch", false);
decltype(Settings::enchantmentCraftedPatch)			Settings::enchantmentCraftedPatch("enchantmentCraftedPatch", false);
decltype(Settings::enchantingMenuExitPatch)			Settings::enchantingMenuExitPatch("enchantingMenuExitPatch", true);
decltype(Settings::poisonPatch)						Settings::poisonPatch("poisonPatch", true);
