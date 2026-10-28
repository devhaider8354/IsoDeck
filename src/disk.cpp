// IsoDeck — physical disk enumeration
#include "disk.h"
#include <initguid.h>
#include <winioctl.h>
#include <cstring>
#include <setupapi.h>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")

namespace isodeck {

Str DiskInfo::Title() const {
    Str m = Trim(model);
    if (m.empty()) m = Format(L"Disk %d", number);
    return m + L" (" + HumanSize(sizeBytes) + L")";
}

Str VolumeLabel(const Str& letter) {
    wchar_t label[MAX_PATH] = {};
    Str root = letter.substr(0, 2) + L"\\";
    GetVolumeInformationW(root.c_str(), label, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);
    return label;
}
uint64_t VolumeFree(const Str& letter) {
    ULARGE_INTEGER freeB{}, total{}, totalFree{};
    Str root = letter.substr(0, 2) + L"\\";
    if (!GetDiskFreeSpaceExW(root.c_str(), &freeB, &total, &totalFree)) return 0;
    return freeB.QuadPart;
}

static int DiskNumberOfVolume(const Str& volPath /* "\\.\E:" or "\\?\Volume{..}" without trailing slash */, int* partition) {
    HANDLE h = CreateFileW(volPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -1;
    STORAGE_DEVICE_NUMBER sdn{}; DWORD cb = 0;
    int n = -1;
    if (DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &sdn, sizeof(sdn), &cb, nullptr) && sdn.DeviceType == FILE_DEVICE_DISK) {
        n = (int)sdn.DeviceNumber;
        if (partition) *partition = (int)sdn.PartitionNumber;
    }
    CloseHandle(h);
    return n;
}

// Ventoy fingerprint: partition #2 is a 32 MB EFI partition (65536 sectors of 512 B).
static bool LooksLikeVentoyLayout(HANDLE hDisk, bool* gptOut) {
    unsigned char sec[512 * 34] = {};
    DWORD rd = 0;
    LARGE_INTEGER zero{}; SetFilePointerEx(hDisk, zero, nullptr, FILE_BEGIN);
    if (!ReadFile(hDisk, sec, sizeof(sec), &rd, nullptr) || rd < 512) return false;
    if (sec[510] != 0x55 || sec[511] != 0xAA) return false;
    const unsigned char* p2 = sec + 446 + 16;      // MBR entry #2
    unsigned char type1 = sec[446 + 4];
    bool gpt = (type1 == 0xEE);
    if (gptOut) *gptOut = gpt;
    if (!gpt) {
        uint32_t sectors = *(const uint32_t*)(p2 + 12);
        unsigned char type = p2[4];
        return sectors == 65536 && (type == 0xEF || type == 0x0B || type == 0x0C || type == 0x01);
    }
    if (rd < 512 * 34) return false;
    // GPT header at LBA1, entries at LBA2 (128 bytes each). Check entry #2.
    if (memcmp(sec + 512, "EFI PART", 8) != 0) return false;
    const unsigned char* e = sec + 1024 + 128;
    uint64_t first = *(const uint64_t*)(e + 32), last = *(const uint64_t*)(e + 40);
    return last > first && (last - first + 1) == 65536;
}

bool IsVentoyDataVolume(const Str& letter, Str* versionOut) {
    int part = 0;
    int n = DiskNumberOfVolume(L"\\\\.\\" + letter.substr(0, 2), &part);
    if (n < 0) return false;
    Str phys = Format(L"\\\\.\\PhysicalDrive%d", n);
    HANDLE h = CreateFileW(phys.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = LooksLikeVentoyLayout(h, nullptr);
    CloseHandle(h);
    if (versionOut) versionOut->clear();
    return ok && part == 1;
}

// Try to read "ventoy\version" from the hidden EFI partition (#2) of a disk.
static Str ReadVentoyVersion(int diskNumber) {
    wchar_t vol[MAX_PATH];
    HANDLE f = FindFirstVolumeW(vol, MAX_PATH);
    if (f == INVALID_HANDLE_VALUE) return L"";
    Str ver;
    do {
        Str v = vol; if (!v.empty() && v.back() == L'\\') v.pop_back();
        int part = 0;
        if (DiskNumberOfVolume(v, &part) == diskNumber && part == 2) {
            std::string raw;
            if (ReadFileBytes(v + L"\\ventoy\\version", raw)) ver = Trim(Utf8ToWide(raw));
            break;
        }
    } while (FindNextVolumeW(f, vol, MAX_PATH));
    FindVolumeClose(f);
    return ver;
}

std::vector<DiskInfo> EnumerateDisks(bool includeNonUsb) {
    std::vector<DiskInfo> disks;

    // System disk number
    int systemDisk = -1;
    {
        wchar_t win[MAX_PATH]; GetWindowsDirectoryW(win, MAX_PATH);
        Str letter(win, 2);
        systemDisk = DiskNumberOfVolume(L"\\\\.\\" + letter, nullptr);
    }

    // Letters -> disk numbers
    struct LetterInfo { Str letter; int disk; int part; };
    std::vector<LetterInfo> letters;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i))) continue;
        Str letter = Format(L"%c:", L'A' + i);
        UINT type = GetDriveTypeW((letter + L"\\").c_str());
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE) continue;
        int part = 0;
        int n = DiskNumberOfVolume(L"\\\\.\\" + letter, &part);
        if (n >= 0) letters.push_back({ letter, n, part });
    }

    HDEVINFO devs = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devs == INVALID_HANDLE_VALUE) return disks;
    SP_DEVICE_INTERFACE_DATA ifd{ sizeof(ifd) };
    for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(devs, nullptr, &GUID_DEVINTERFACE_DISK, idx, &ifd); ++idx) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailW(devs, &ifd, nullptr, 0, &need, nullptr);
        std::vector<char> buf(need + 8);
        auto* detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*)buf.data();
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(devs, &ifd, detail, need, nullptr, nullptr)) continue;

        HANDLE h = CreateFileW(detail->DevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;

        DiskInfo d;
        DWORD cb = 0;
        STORAGE_DEVICE_NUMBER sdn{};
        if (DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &sdn, sizeof(sdn), &cb, nullptr)) d.number = (int)sdn.DeviceNumber;

        STORAGE_PROPERTY_QUERY q{ StorageDeviceProperty, PropertyStandardQuery };
        std::vector<char> pb(4096);
        if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), pb.data(), (DWORD)pb.size(), &cb, nullptr)) {
            auto* desc = (STORAGE_DEVICE_DESCRIPTOR*)pb.data();
            d.isUsb = desc->BusType == BusTypeUsb;
            d.isRemovable = desc->RemovableMedia != 0;
            Str vendor = desc->VendorIdOffset ? Utf8ToWide(pb.data() + desc->VendorIdOffset) : L"";
            Str product = desc->ProductIdOffset ? Utf8ToWide(pb.data() + desc->ProductIdOffset) : L"";
            d.serial = desc->SerialNumberOffset ? Trim(Utf8ToWide(pb.data() + desc->SerialNumberOffset)) : L"";
            d.model = Trim(Trim(vendor) + L" " + Trim(product));
        }
        GET_LENGTH_INFORMATION len{};
        if (DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &len, sizeof(len), &cb, nullptr)) d.sizeBytes = (uint64_t)len.Length.QuadPart;
        d.ventoyInstalled = LooksLikeVentoyLayout(h, &d.gpt);
        CloseHandle(h);

        if (d.number < 0 || d.sizeBytes == 0) continue;
        d.isSystem = (d.number == systemDisk);
        if (!includeNonUsb && !d.isUsb) continue;
        if (d.isSystem && !includeNonUsb) continue;

        for (auto& l : letters) if (l.disk == d.number) {
            d.letters.push_back(l.letter);
            if (l.part == 1 && d.dataLetter.empty()) d.dataLetter = l.letter;
        }
        if (d.dataLetter.empty() && !d.letters.empty()) d.dataLetter = d.letters[0];
        if (!d.dataLetter.empty()) d.freeBytes = VolumeFree(d.dataLetter);
        if (d.ventoyInstalled) d.ventoyVersion = ReadVentoyVersion(d.number);
        disks.push_back(d);
    }
    SetupDiDestroyDeviceInfoList(devs);
    std::sort(disks.begin(), disks.end(), [](const DiskInfo& a, const DiskInfo& b) { return a.number < b.number; });
    return disks;
}

} // namespace isodeck
