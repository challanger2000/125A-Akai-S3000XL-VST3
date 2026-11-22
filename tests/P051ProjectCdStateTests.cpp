#include "Source/AkaiProjectCdState.h"
#include <cstdlib>
#include <iostream>
#include <string>
static bool expect(bool cond,const char* name) {
    if(!cond) std::cerr<<"P051 FAIL "<<name<<"\n";
    return cond;
}
int main() {
    using akai::cd_project::resolve;
    bool ok=true;
    const auto legacy=resolve(false,false,false,"last-used-global.iso",false);
    ok &= expect(legacy.valid && !legacy.present,"legacy_preserves_global");
    const auto inserted=resolve(true,true,true,"User/Akai Library.iso",true);
    ok &= expect(inserted.valid && inserted.present && inserted.mount
                 && inserted.path=="User/Akai Library.iso","owner_cd_reopened");
    const auto missing=resolve(true,true,true,"old.iso",false);
    ok &= expect(missing.valid && missing.present && !missing.mount
                 && missing.missing && missing.path.empty(),"missing_cd_keeps_native_ram");
    const auto eject=resolve(true,true,false,"stale.iso",true);
    ok &= expect(eject.valid && eject.present && !eject.mount
                 && eject.path.empty(),"explicit_eject");
    const auto corrupt=resolve(true,false,true,"bad.iso",true);
    ok &= expect(!corrupt.valid,"partial_xml_rejected");
    const auto inverse=resolve(false,true,true,"bad.iso",true);
    ok &= expect(!inverse.valid,"missing_path_pair_rejected");
    const auto invalid=resolve(true,true,true,std::string("a\0b",3),true);
    ok &= expect(!invalid.valid,"nul_path_rejected");
    const auto overlong=resolve(true,true,true,std::string(4097,'x'),true);
    ok &= expect(!overlong.valid,"overlong_path_rejected");
    const auto empty=resolve(true,true,true,"",false);
    ok &= expect(empty.valid && empty.missing && !empty.mount,"missing_empty_path");
    if(!ok) return EXIT_FAILURE;
    std::cout<<"P051_PROJECT_CD_MEDIA_9_CASES_PASS\n";
    return EXIT_SUCCESS;
}
