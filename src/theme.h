// IsoDeck — boot-menu theme + ventoy.json generation
#pragma once
#include "util.h"

namespace isodeck {

struct AliasEntry { Str relPath; Str alias; Str unattendRel; };   // relPath like "/ISO/win11.iso"

struct ThemeSettings {
    Str  brand = L"ISODECK";
    Str  accent = L"#00A3FF";      // hex
    Str  backgroundPath;           // custom image on PC (empty = embedded default)
    int  timeoutSec = 30;
    bool win11Bypass = true;
    std::vector<AliasEntry> aliases;

    void LoadFrom(const Config& c);
    void SaveTo(Config& c) const;
};

// Writes  <drive>\ventoy\theme\isodeck\{theme.txt,background.png|jpg}  and  <drive>\ventoy\ventoy.json
bool ApplyTheme(const Str& driveLetter, const ThemeSettings& t, Str* error);

// Reads existing aliases from an existing ventoy.json is out of scope (we keep them in our config per drive serial).
Str  DriveRelPath(const Str& driveLetter, const Str& fullPath);   // "E:\ISO\a.iso" -> "/ISO/a.iso"

} // namespace isodeck
