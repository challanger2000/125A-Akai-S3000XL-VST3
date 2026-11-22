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
    // Tuth/S3000XL-VST: the original MAME S3000XL wiring connects a
    // Sony CDU-76S 2x drive to SCSI ID 4 and the optional HDD to ID 5.
    // Keep the CD device at target 4 even with no disc so project state
    // remains compatible with saved original-machine device identity.
    args.emplace_back("-scsi:4");
    args.emplace_back("cdrom_2x");
    args.emplace_back("-scsi:5");
    if (!hardDiskImage.empty()) {
        args.emplace_back("harddisk");
        args.emplace_back("-hard");
        args.push_back(hardDiskImage);
    } else {
        // MAME 0.288 defaults target 5 to a harddisk even with no actual
        // HDD image: suppress that phantom device for a CD-only sampler.
        args.emplace_back("");
    }
    if (hasCD) {
        args.emplace_back("-cdrm");
        args.push_back(cdImage);
    }
}
} // namespace akai
