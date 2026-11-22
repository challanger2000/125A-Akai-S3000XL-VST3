#include "Source/AkaiScsiMediaPlan.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static bool matches(const std::vector<std::string>& got,
                    const std::vector<std::string>& expected,
                    const char* caseName)
{
    if (got == expected) return true;
    std::cerr << "FAIL " << caseName << ": actual=";
    for (const auto& part:got) std::cerr << '[' << part << ']';
    std::cerr << ", expected=";
    for (const auto& part:expected) std::cerr << '[' << part << ']';
    std::cerr << '\n';
    return false;
}
int main()
{
    struct Case {
        const char* name;
        std::string cd, hdd;
        std::vector<std::string> expected;
    };
    const Case cases[] = {
        {"CD_only_original_Tuth_CD4", "akai.iso", "",
            {"-scsi:4","cdrom_2x","-scsi:5","","-cdrm","akai.iso"}},
        {"CD_only_nonASCII_source_path", "Akai/Bibliothek 1.iso", "",
            {"-scsi:4","cdrom_2x","-scsi:5","","-cdrm","Akai/Bibliothek 1.iso"}},
        {"no_media_CD4_device_HDD5_disabled", "", "",
            {"-scsi:4","cdrom_2x","-scsi:5",""}},
        {"HDD_only_original_slot5", "", "hard.chd",
            {"-scsi:4","cdrom_2x","-scsi:5","harddisk","-hard","hard.chd"}},
        {"HDD_plus_CD_preserve_existing_mapping", "akai.iso", "hard.chd",
            {"-scsi:4","cdrom_2x","-scsi:5","harddisk","-hard","hard.chd","-cdrm","akai.iso"}}
    };
    for (const auto& c : cases) {
        std::vector<std::string> args;
        akai::appendScsiMediaArguments(args,c.cd,c.hdd);
        if (!matches(args,c.expected,c.name)) return EXIT_FAILURE;
    }
    // P050 regression: rejecting the unverified CD-only ID5 routing that
    // failed to load the original owner Akai library in Studio Pro.
    const std::vector<std::string> old={"-scsi:4","","-scsi:5","cdrom_2x","-cdrm","akai.iso"};
    std::vector<std::string> correct;
    akai::appendScsiMediaArguments(correct,"akai.iso","");
    if (correct==old) {
        std::cerr << "FAIL prior broken CD-only mapping passed\n";
        return EXIT_FAILURE;
    }

    // P053: the SAME emulated SCSI device must persist independently of
    // medium presence. Otherwise MAME refuses the saved original-machine
    // image and factory-only MIDI plays while CD-loaded programs disappear.
    for (const auto& hard : {std::string(),std::string("hard.chd")}) {
        std::vector<std::string> empty, inserted;
        akai::appendScsiMediaArguments(empty,"",hard);
        akai::appendScsiMediaArguments(inserted,"akai.iso",hard);
        if (empty.size()<4 || inserted.size()<4 ||
            !std::equal(empty.begin(),empty.begin()+4,inserted.begin()) ||
            empty[1]!="cdrom_2x") {
            std::cerr << "FAIL P053 optical-device changes on eject or missing ISO\n";
            return EXIT_FAILURE;
        }
    }
    // Deliberate negative: the previous P050 no-disc plain-cdrom mapping
    // violates this requirement, even though it booted normally.
    const std::vector<std::string> oldNoDisc={"-scsi:4","cdrom","-scsi:5",""};
    std::vector<std::string> currentNoDisc;
    akai::appendScsiMediaArguments(currentNoDisc,"","");
    if (currentNoDisc==oldNoDisc) {
        std::cerr << "FAIL P053 old changing-CD-device regression survived\n";
        return EXIT_FAILURE;
    }
    std::cout << "P061_UPSTREAM_SCSI_CD4_HDD5_DISABLED_WHEN_MISSING_P050_OLD_CD5_REJECTED\\n";
    return EXIT_SUCCESS;
}
