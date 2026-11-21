#include "../Source/AkaiSdsBootGate.h"
#include <cstring>
#include <iostream>
#include <string>
#define CHECK(x) do{if(!(x)){std::cerr<<"P035 FAIL "#x" line "<<__LINE__<<'\n';return 1;}}while(0)
std::string lcd(const char* a,const char* b=""){
 std::string s(320,' ');
 if(std::strlen(a)>40 || std::strlen(b)>40)throw 1;
 s.replace(0,std::strlen(a),a);
 s.replace(40,std::strlen(b),b);
 return s;
}
int main(){
 using namespace akai::sds;
 const auto boot=lcd("AKAI PROFESSIONAL OPERATING SYSTEM 2.00");
 const auto wait=lcd("waiting for hard disk ready.. SKIP");
 const auto ready=lcd("PROGRAMS IN MEMORY  SLCT", "1 TEST PROGRAM");
 const auto unready=lcd("PROGRAMS IN MEMORY","1 TEST PROGRAM");
 BootGate gate;CHECK(gate.start(0));
 auto d=gate.step(boot,true,true,1500);CHECK(d.action==BootAction::None);
 d=gate.step(unready,true,true,2000);CHECK(!gate.ready());
 d=gate.step(wait,false,true,3000);CHECK(d.action==BootAction::None);
 d=gate.step(wait,true,true,3500);CHECK(d.action==BootAction::PressF8);
 d=gate.step(wait,true,true,3650);CHECK(d.action==BootAction::None);
 d=gate.step(wait,true,true,4040);CHECK(d.action==BootAction::ReleaseF8);
 d=gate.step(wait,true,true,5200);CHECK(d.action==BootAction::None);
 d=gate.step(wait,true,true,6700);CHECK(d.action==BootAction::PressF8);
 d=gate.step(wait,true,true,7250);CHECK(d.action==BootAction::ReleaseF8);
 d=gate.step(ready,true,true,7500);CHECK(gate.ready() && d.action==BootAction::None);
 CHECK(gate.step(wait,true,true,8500).action==BootAction::None);
 BootGate bad;CHECK(bad.start(100));
 CHECK(bad.step(boot,false,false,200).phase==BootPhase::Failed);
 CHECK(bad.start(0));
 CHECK(bad.step(boot,true,true,120001).phase==BootPhase::Failed);
 BootGate noSkip;CHECK(noSkip.start(0));
 CHECK(noSkip.step(lcd("waiting for hard disk ready.."),true,true,1500).action==BootAction::None);
 CHECK(!noSkip.ready());
 BootGate held;CHECK(held.start(0));
 CHECK(held.step(wait,true,true,100).action==BootAction::PressF8);
 CHECK(held.fail().action==BootAction::ReleaseF8);
 std::cout<<"P035_ORIGINAL_AKAI_ROM_BOOT_SKIP_GATE=PASS\n";
}
