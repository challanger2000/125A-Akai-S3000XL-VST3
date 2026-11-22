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
    // P062: original firmware V2.0 selects SCSI ID 5 after a power cycle.
    // The upstream Tuth CD-ROM target 4 requires manually choosing ID 4
    // on the Akai panel. For the owner's one-click CD-only workflow, attach
    // the 2x optical drive to the FIRMWARE-SELECTED target 5, not target 4.
    // An image is provided via -cdrm before the MAME machine is booted,
    // precisely as in the upstream verified startup path.
    if (hardDiskImage.empty()) {
        args.emplace_back("-scsi:4");
        args.emplace_back("");         // disable the upstream CD4 slot
        args.emplace_back("-scsi:5");
        args.emplace_back("cdrom_2x");  // firmware default SCSI ID5
    } else {
        // Preserve upstream CD4 and HDD5 when a real HDD is installed.
        // Selecting CD4 within the original firmware is a separate task.
        args.emplace_back("-scsi:4");
        args.emplace_back("cdrom_2x");
        args.emplace_back("-scsi:5");
        args.emplace_back("harddisk");
        args.emplace_back("-hard");
        args.push_back(hardDiskImage);
    }
    if (hasCD) {
        args.emplace_back("-cdrm");
        args.push_back(cdImage);
    }
}
} // namespace akai
