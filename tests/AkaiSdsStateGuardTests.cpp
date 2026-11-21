#include "../Source/AkaiSdsStateGuard.h"
#include <iostream>
#define CHECK(expr) do {if(!(expr)){std::cerr<<"P036 FAIL: "#expr<<" line "<<__LINE__<<'\n';return 1;}} while(false)
int main(){
 using namespace akai::sds;
 CHECK(safeToSnapshotOriginalFirmware(0));
 CHECK(safeToSnapshotOriginalFirmware(6));
 for(const int stage : {1,2,3,4,5,7}) {
  CHECK(!safeToSnapshotOriginalFirmware(stage));
  CHECK(requiresOriginalFirmwareWarning(stage));
 }
 for(const int stage : {-1,8,15,9999})
  CHECK(!safeToSnapshotOriginalFirmware(stage));
 CHECK(!requiresOriginalFirmwareWarning(0));
 CHECK(!requiresOriginalFirmwareWarning(6));
 std::cout<<"P036_ORIGINAL_SDS_INCOMPLETE_STATE_NEVER_FALSE_PASS=PASS\n";
}
