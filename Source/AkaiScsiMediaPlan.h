#pragma once
#include <string>
#include <vector>

// Native MAME command-line media attachment, on the non-realtime worker only.
namespace akai {
inline void appendScsiMediaArguments(std::vector<std::string>& args,
                                     const std::string& cdImage,
                                     const std::string& hardDiskImage)
{
    const bool hasCD = !cdImage.empty();
    const bool hasHDD = !hardDiskImage.empty();
    if (hasHDD) {
        // Preserve HDD at target5 and optional CD at target4.
        // P053: optical device type is fixed across empty/loaded discs so
        // a saved native MAME machine state can be restored without the ISO.
        args.emplace_back("-scsi:4");
        args.emplace_back("cdrom_2x");
        args.emplace_back("-scsi:5");
        args.emplace_back("harddisk");
        args.emplace_back("-hard");
        args.push_back(hardDiskImage);
    } else {
        // Original Akai V2.0 factory drive SCSI ID5; no phantom HDD.
        // P053: always cdrom_2x, even when ejected / original ISO missing.
        // Exact original-ROM state with cdrom_2x fails to restore into plain
        // cdrom (confirmed by false factory-only MIDI replay).
        // Explicitly disable the MAME 0.288 default CD-ROM on target4.
        args.emplace_back("-scsi:4");
        args.emplace_back("");
        args.emplace_back("-scsi:5");
        args.emplace_back("cdrom_2x");
    }
    if (hasCD) {
        args.emplace_back("-cdrm");
        args.push_back(cdImage);
    }
}
} // namespace akai
