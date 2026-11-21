#include "../Source/AkaiInstanceAudioState.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <thread>
#define CHECK(x) do {if(!(x)){std::cerr<<"P012 FAIL "<<#x<<" line "<<__LINE__<<'\n';return 1;}}while(false)
int main(){
  using akai::runtime::AudioState;
  AudioState a,b;
  a.output.begin(11);
  a.output.observe(":floppy",0,1);
  a.output.observe(":speaker",1,2);
  a.output.observe(":indiv",3,8);
  b.output.begin(11);
  b.output.observe(":speaker",0,2);
  b.output.observe(":floppy",2,1);
  b.output.observe(":indiv",3,8);
  CHECK(a.output.hasMain() && b.output.hasMain());
  CHECK(a.output.hasIndividuals() && b.output.hasIndividuals());
  CHECK(a.output.main==1 && b.output.main==0);
  std::array<std::int16_t,11> pcm={100,200,300,0,0,0,0,0,0,0,0};
  CHECK(pcm[a.output.main]==200 && pcm[b.output.main]==100);
  AudioState bad;
  bad.output.begin(5);
  bad.output.observe(":speaker",4,2);
  bad.output.observe(":indiv",0,8);
  CHECK(!bad.output.hasMain() && !bad.output.hasIndividuals());
  for(int i=0;i<63;++i)CHECK(a.drift.next(11000,1)==0);
  CHECK(b.drift.next(11000,1)==0);
  CHECK(a.drift.next(11000,1)==1);
  CHECK(a.drift.tick==64 && b.drift.tick==1);
  for(int i=0;i<62;++i)CHECK(b.drift.next(0,1)==0);
  CHECK(b.drift.next(0,1)==-1);
  CHECK(a.drift.tick==64 && b.drift.tick==64);
  std::uint32_t oldSharedTick=63;
  CHECK(((++oldSharedTick)&63u)==0u); // counterfactual old global timer
  a.meter.calls=2000; a.meter.lastW=1000;
  CHECK(b.meter.calls==0 && b.meter.lastW==0);
  a.reset();
  CHECK(a.output.stride==0 && a.output.main==-1 && a.drift.tick==0);
  CHECK(a.meter.calls==0 && a.meter.lastW==0);
  CHECK(b.output.stride==11 && b.drift.tick==64);
  std::thread ta([&]{for(int i=0;i<10000;++i)a.drift.next(11000,1);});
  std::thread tb([&]{for(int i=0;i<10000;++i)b.drift.next(0,1);});
  ta.join();tb.join();
  CHECK(a.drift.tick==10000 && b.drift.tick==10064);
  std::cout<<"P012_INSTANCE_AUDIO_MAP_AND_DRIFT=PASS\n";
}
