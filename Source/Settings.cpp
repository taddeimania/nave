#include "Settings.h"

namespace nave
{
namespace
{
juce::PropertiesFile::Options options()
{
    juce::PropertiesFile::Options o;
    o.applicationName         = "Nave";
    o.filenameSuffix          = ".settings";
   #if JUCE_LINUX
    o.folderName              = ".config/Nave";
   #else
    o.folderName              = "Nave";
   #endif
    o.osxLibrarySubFolder     = "Application Support";
    o.storageFormat           = juce::PropertiesFile::storeAsXML;
    o.millisecondsBeforeSaving = -1;   // every change is saved explicitly
    return o;
}

constexpr const char* kTooltips = "tooltips";
} // namespace

SettingsStore::SettingsStore() : props (options()) {}

juce::File Settings::file() { return options().getDefaultFile(); }

bool Settings::tooltipsEnabled() const
{
    return store->props.getBoolValue (kTooltips, true);
}

void Settings::setTooltipsEnabled (bool on)
{
    store->props.setValue (kTooltips, on);
    store->props.saveIfNeeded();
}
} // namespace nave
