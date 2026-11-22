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
        // Preserve existing HDD ID5 and optional CD ID4 project mapping.
        args.emplace_back("-scsi:4");
        args.emplace_back(hasCD ? "cdrom_2x" : "cdrom");
        args.emplace_back("-scsi:5");
        args.emplace_back("harddisk");
        args.emplace_back("-hard");
        args.push_back(hardDiskImage);
    } else {
        // Original Akai V2.0 factory drive SCSI ID5; no phantom HDD.
        // Explicitly disable the MAME 0.288 default CD-ROM on target4.
        args.emplace_back("-scsi:4");
        args.emplace_back("");
        args.emplace_back("-scsi:5");
        args.emplace_back(hasCD ? "cdrom_2x" : "cdrom");
    }
    if (hasCD) {
        args.emplace_back("-cdrm");
        args.push_back(cdImage);
    }
}
} // namespace akai
