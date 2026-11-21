#include "../Source/AkaiSdsAssignment.h"
#include <array>
#include <cstring>
#include <iostream>
#include <string>
#define CK(x) do{if(!(x)){std::cerr<<"P031 FAIL "#x" line "<<__LINE__<<'\n';return 1;}}while(0)
std::string lcd(const char* x,const char* y="",const char* z="") {
 std::string s(320,' ');
 for(const auto [at,text]:std::array<std::pair<int,const char*>,3>{std::pair{0,x},{40,y},{80,z}}) {
  if(std::strlen(text)>40)throw 1;
  s.replace(at,std::strlen(text),text);
 }
 return s;
}
int main(){
 using namespace akai::sds;
 auto select=lcd("PROGRAMS IN MEMORY  SLCT", "1 TEST PROGRAM");
 auto edit=lcd("PROGRAM EDIT  TEST PROGRAM", "KEYGROUPS: 1");
 auto keygroup=lcd("KEYGROUPS IN PROGRAM:   1", "ACTIVE KEYGROUP NUMBER:   1  SMPL", "TEST PROGRAM");
 auto sine=lcd("KG: 1 ED:ONE TEST PROGRAM", "zone sample V-LO V-HI", "1 SINE           0 127  SMP1");
 auto midi=lcd("KG: 1 ED:ONE TEST PROGRAM", "zone sample V-LO V-HI", "1 MIDI      99   0 127  SMP1");
 // Real native LCD is 8x40; sample name is padded between MIDI and 99.
 CK(!validOriginalZoneOne(sine));
 CK(validOriginalZoneOne(midi));
 auto bad=midi;
 bad.replace(80+13,2,"? ");CK(!validOriginalZoneOne(bad));
 auto wrong=midi;
 wrong.replace(80,1,"2");CK(!validOriginalZoneOne(wrong));
 CK(!validOriginalZoneOne(lcd("other"),"MIDI 99"));
 auto other=keygroup;
 other.replace(23,1,"2");CK(!akai::capture::originalKeygroupsPage(other));
 Assignment a;CK(a.start(0));
 Decision d=a.step(select,true,true,100);
 CK(d.action==Action::Down && d.button==Button::Single);
 d=a.step(select,true,true,450);
 CK(d.action==Action::Up && d.stage==Phase::WaitSingle);
 d=a.step(select,true,true,600);
 CK(d.action==Action::Down && d.button==Button::Edit);
 d=a.step(edit,true,true,970);CK(d.action==Action::Up);
 d=a.step(edit,true,true,1100);
 CK(d.action==Action::Down && d.button==Button::F2);
 d=a.step(keygroup,true,true,1500);CK(d.action==Action::Up);
 d=a.step(keygroup,true,true,1600);
 CK(d.action==Action::Down && d.button==Button::F6);
 d=a.step(sine,true,true,1980);CK(d.action==Action::Up);
 d=a.step(sine,true,true,2000);CK(d.action==Action::DialIncrement);
 d=a.step(sine,true,true,2500);CK(d.action==Action::None);
 d=a.step(sine,true,true,2670);CK(d.action==Action::DialIncrement);
 d=a.step(midi,true,true,3000);CK(d.stage==Phase::Verified && a.verified());
 CK(!a.running());
 Assignment fault;CK(fault.start(0));
 CK(fault.step(select,false,true,100).stage==Phase::Failed);
 CK(fault.start(200));
 CK(fault.step(select,true,true,9201).stage==Phase::Failed);
 std::cout<<"P031_OWNER_AKAI_LCD_KEYGROUP_ASSIGNMENT_GUARD=PASS\n";
}
