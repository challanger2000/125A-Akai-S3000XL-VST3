#include "Source/AkaiScsiMediaPlan.h"
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
        {"CD_only_original_Akai_drive5", "akai.iso", "",
            {"-scsi:4","","-scsi:5","cdrom_2x","-cdrm","akai.iso"}},
        {"CD_only_nonASCII_source_path", "Akai/Bibliothek 1.iso", "",
            {"-scsi:4","","-scsi:5","cdrom_2x","-cdrm","Akai/Bibliothek 1.iso"}},
        {"no_media_prepared_hot_swap_target5", "", "",
            {"-scsi:4","","-scsi:5","cdrom"}},
        {"HDD_only_original_slot5", "", "hard.chd",
            {"-scsi:4","cdrom","-scsi:5","harddisk","-hard","hard.chd"}},
        {"HDD_plus_CD_preserve_existing_mapping", "akai.iso", "hard.chd",
            {"-scsi:4","cdrom_2x","-scsi:5","harddisk","-hard","hard.chd","-cdrm","akai.iso"}}
    };
    for (const auto& c : cases) {
        std::vector<std::string> args;
        akai::appendScsiMediaArguments(args,c.cd,c.hdd);
        if (!matches(args,c.expected,c.name)) return EXIT_FAILURE;
    }
    // Historical target4-only setting is an intentionally rejected mutant.
    const std::vector<std::string> old={"-scsi:4","cdrom_2x","-cdrm","akai.iso"};
    std::vector<std::string> correct;
    akai::appendScsiMediaArguments(correct,"akai.iso","");
    if (correct==old) {
        std::cerr << "FAIL prior broken CD-only mapping passed\n";
        return EXIT_FAILURE;
    }
    std::cout << "P050_SCSI_MEDIA_PLAN_5_CASES_PASS_ID4_NEGATIVE_REJECTED\n";
    return EXIT_SUCCESS;
}
