#include "../Source/AkaiMidiSampleDump.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<"P030 SDS failed at line "<<__LINE__<<": "#x"\n";return 1;}}while(0)
int main(){
 using namespace akai::sds;
 const std::array<std::int16_t,5> pcm={-32768,-1,0,1,32767};
 const auto d=encodeMono(pcm,44100,99,0);
 constexpr std::array<std::uint8_t,21> expected={
  0xf0,0x7e,0x00,0x01,0x63,0x00,0x10,0x14,0x31,0x01,
  0x05,0,0,0x05,0,0,0x05,0,0,0x7f,0xf7
 };
 CHECK(d.sysex.size()==148 && std::equal(expected.begin(),expected.end(),d.sysex.begin()));
 constexpr std::array<std::uint8_t,15> words={
  0,0,0,0x3f,0x7f,0x60,0x40,0,0,0x40,0,0x20,0x7f,0x7f,0x60
 };
 CHECK(std::equal(words.begin(),words.end(),d.sysex.begin()+26));
 for(std::size_t i=41;i<146;++i)CHECK(d.sysex[i]==0);
 CHECK(d.sysex[21]==0xf0 && d.sysex[22]==0x7e);
 CHECK(d.sysex[23]==0 && d.sysex[24]==2 && d.sysex[25]==0);
 CHECK(d.sysex[147]==0xf7);
 std::uint8_t checksum=0;
 for(std::size_t i=22;i<146;++i)checksum^=d.sysex[i];
 CHECK(d.sysex[146]==checksum);
 CHECK(d.transportMillis>=1000 && d.transportMillis<1500);
 std::vector<std::int16_t> frame(40*129,-10000);
 const auto wrap=encodeMono(frame,44100);
 CHECK(wrap.packets()==129 && wrap.sysex.size()==21+129*127);
 for(std::size_t p=0;p<wrap.packets();++p){
  const auto at=21+127*p;
  CHECK(wrap.sysex[at]==0xf0 && wrap.sysex[at+1]==0x7e);
  CHECK(wrap.sysex[at+3]==2 && wrap.sysex[at+4]==(p&127));
  CHECK(wrap.sysex[at+126]==0xf7);
  std::uint8_t check=0;
  for(std::size_t j=at+1;j<at+125;++j)check^=wrap.sysex[j];
  CHECK(check==wrap.sysex[at+125]);
 }
 const std::array<std::int16_t,8> stereo={10000,-10000,32767,32767,-32768,-32768,0,1};
 const auto down=encodeStereoAsMono(stereo,44100);
 CHECK(down.frames==4);
 CHECK(down.sysex[26]==0x40 && down.sysex[27]==0 && down.sysex[28]==0);
 CHECK(down.sysex[29]==0x7f && down.sysex[30]==0x7f && down.sysex[31]==0x60);
 CHECK(down.sysex[32]==0 && down.sysex[33]==0 && down.sysex[34]==0);
 CHECK(down.sysex[35]==0x40 && down.sysex[36]==0 && down.sysex[37]==0);
 auto bad=[&](auto f){try{f();return false;}catch(const std::invalid_argument&){return true;}};
 CHECK(bad([&]{encodeMono({},44100);}));
 CHECK(bad([&]{encodeMono(pcm,0);}));
 CHECK(bad([&]{encodeMono(pcm,192000);}));
 CHECK(bad([&]{encodeMono(pcm,44100,16384);}));
 CHECK(bad([&]{encodeMono(pcm,44100,99,128);}));
 CHECK(bad([&]{encodeStereoAsMono(std::array<std::int16_t,1>{5},44100);}));
 // From independent owner-S3000XL V2.0 firmware native control test,
 // accepted sample called 'MIDI 99'; device ID 127 FAILED, ID 0 PASSED.
 const auto owner=encodeMono(std::vector<std::int16_t>(800),44100,99,0);
 constexpr std::array<std::uint8_t,21> ownerHeader={
  0xf0,0x7e,0x00,0x01,0x63,0,0x10,0x14,0x31,0x01,
  0x20,0x06,0,0x20,0x06,0,0x20,0x06,0,0x7f,0xf7
 };
 CHECK(std::equal(ownerHeader.begin(),ownerHeader.end(),owner.sysex.begin()));
 CHECK(owner.packets()==20 && owner.sysex.size()==2561);
 // P032: 42 ticks = 43.75ms at 480 PPQN 120BPM, safely above
 // real 127-byte MIDI UART wire time 40.64ms. Native original owner Akai
 // accepted timed-MIDI SDS, confirmed actual LCD + audible state reopen.
 const auto midiFile=toTimedMidiFile(owner);
 CHECK(midiFile.size()>owner.sysex.size());
 constexpr std::array<std::uint8_t,14> midiHeader={
  'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0};
 CHECK(std::equal(midiHeader.begin(),midiHeader.end(),midiFile.begin()));
 CHECK(midiFile[14]=='M' && midiFile[15]=='T' && midiFile[16]=='r'
       && midiFile[17]=='k');
 CHECK(midiFile[22]==0 && midiFile[23]==0xff && midiFile[24]==0x51);
 CHECK(midiFile[29]==0 && midiFile[30]==0xf0 && midiFile[31]==20);
 CHECK(midiFile[52]==10 && midiFile[53]==0xf0 && midiFile[54]==126);
 CHECK(midiFile.back()==0 && midiFile[midiFile.size()-4]==0);
 CHECK(midiFile[midiFile.size()-3]==0xff);
 CHECK(midiFile[midiFile.size()-2]==0x2f);
 CHECK(timedMidiTransferMillis(owner)>2000 && timedMidiTransferMillis(owner)<3000);
 // 65 real data packets: 64 share one UART event, the next one MUST
 // be delayed at least 2500 MIDI ticks to protect 131072-byte MAME TX ring.
 const auto many=encodeMono(std::vector<std::int16_t>(65*40,15000),44100,99,0);
 const auto grouped=toTimedMidiFile(many);
 std::size_t cursor=22; // standard MIDI file 14-byte header + 8-byte MTrk
 auto readVlq=[&]()->std::uint32_t {
  std::uint32_t v=0;
  for(int k=0;k<5;++k) {
   CK(cursor<grouped.size()); // independent malformed MIDI guard
   const auto b=grouped[cursor++];
   v=(v<<7)|(b&127u);
   if(!(b&128u))return v;
  }
  return 0xffffffffu;
 };
 CHECK(grouped[cursor++]==0 && grouped[cursor++]==0xff);
 CHECK(grouped[cursor++]==0x51 && grouped[cursor++]==3);
 cursor+=3; // original 500000 usec per quarter
 std::size_t originalOffset=0;
 for(std::size_t p=0;p<66;++p) {
  const auto delta=readVlq();
  CHECK(delta==(p==0?0u:p==1?10u:p==65?2500u:0u));
  CHECK(grouped[cursor++]==0xf0);
  const auto length=readVlq();
  const auto bytes=std::size_t(p==0?21:127);
  CHECK(length==bytes-1);
  CHECK(std::equal(grouped.begin()+std::ptrdiff_t(cursor),
       grouped.begin()+std::ptrdiff_t(cursor+length),
       many.sysex.begin()+std::ptrdiff_t(originalOffset+1)));
  cursor+=length;originalOffset+=bytes;
 }
 CHECK(grouped[cursor]==0 && grouped[cursor+1]==0xff
       && grouped[cursor+2]==0x2f && grouped[cursor+3]==0);
 CHECK(cursor+4==grouped.size() && originalOffset==many.sysex.size());
 CHECK(timedMidiTransferMillis(many)>2700 &&
       timedMidiTransferMillis(many)<5000);
 Dump broken=owner;broken.sysex.pop_back();
 CHECK(bad([&]{toTimedMidiFile(broken);}));
 std::cout<<"P030_P032_AKAI_SDS_PACKET_AND_FAST_TIMED_MIDI_WIRE=PASS\n";
}
