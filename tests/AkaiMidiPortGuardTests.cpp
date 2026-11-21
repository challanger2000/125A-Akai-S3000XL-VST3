#include "../Source/AkaiMidiPortGuard.h"
#include <iostream>
#include <string>
using namespace std::string_literals;
#define CHECK(e) do {if(!(e)){std::cerr<<"P042 midi input port guard failed: "#e<<"\n";return 1;}}while(false)
int main(){
  using akai::sds::isLiveVstMidiInputPort;
  CHECK(isLiveVstMidiInputPort("VST MIDI"));
  CHECK(!isLiveVstMidiInputPort(""));
  CHECK(!isLiveVstMidiInputPort("VST MIDI "));
  CHECK(!isLiveVstMidiInputPort("VST MIDI/owner-akai-sample-99.mid"));
  CHECK(!isLiveVstMidiInputPort("owner-akai-sample-99.mid"));
  CHECK(!isLiveVstMidiInputPort("C:\\Users\\owner\\sds\\owner-akai-sample-99.mid"));
  CHECK(!isLiveVstMidiInputPort("/tmp/sds/owner-akai-sample-99.mid"));
  CHECK(!isLiveVstMidiInputPort("vst midi"));
  CHECK(!isLiveVstMidiInputPort("VST MIDI\0some.mid"s)); // embedded NUL is not a port alias
  std::cout<<"P042_MIDI_IMAGE_MISSING_FILE_FAIL_CLOSED=PASS\n";
}
