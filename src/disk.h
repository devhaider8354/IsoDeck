// IsoDeck — physical disk enumeration (safe: system disk is flagged and never selectable)
#pragma once
#include "util.h"

namespace isodeck {

struct DiskInfo {
    int       number = -1;          // \\.\PhysicalDriveN
    Str       model;                // "SanDisk Ultra"
    Str       serial;
    uint64_t  sizeBytes = 0;
    bool      isUsb = false;
    bool      isRemovable = false;
    bool      isSystem = false;     // contains the Windows volume
    bool      ventoyInstalled = false;
    Str       ventoyVersion;        // read from the data partition marker if present
    Str       dataLetter;           // "E:" volume of the Ventoy data partition (or first volume)
    std::vector<Str> letters;       // all mounted letters on this disk
    uint64_t  freeBytes = 0;        // free space on dataLetter
    bool      gpt = false;

    Str Title() const;              // "SanDisk Ultra (28.6 GB)"
};

std::vector<DiskInfo> EnumerateDisks(bool includeNonUsb);
bool  IsVentoyDataVolume(const Str& letter, Str* versionOut);
Str   VolumeLabel(const Str& letter);
uint64_t VolumeFree(const Str& letter);

} // namespace isodeck
