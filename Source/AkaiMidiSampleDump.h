// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// The real Akai S3000XL V2.0 firmware receives MIDI Sample Dump Standard.
// This code ONLY encodes genuine external MIDI bytes, no guest RAM or DSP edits.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>
namespace akai::sds {
struct Dump {
 std::vector<std::uint8_t> sysex;
 std::uint32_t frames=0,rate=0;
 std::uint16_t sampleNumber=0;
 std::uint64_t transportMillis=0;
 std::size_t packets() const noexcept {return (frames+39u)/40u;}
};
inline constexpr std::uint32_t maxFrames=2u*1024u*1024u;
inline std::array<std::uint8_t,3> midi21(std::uint32_t n) {
 if(n>=(1u<<21))throw std::invalid_argument("SDS 21bit value out of range");
 return {std::uint8_t(n&127),std::uint8_t((n>>7)&127),std::uint8_t((n>>14)&127)};
}
inline Dump encodeMono(std::span<const std::int16_t> mono,std::uint32_t rate,
                       std::uint16_t number=99,std::uint8_t device=0) {
 if(mono.empty() || mono.size()>maxFrames || rate<8000 || rate>48000
    || number>16383 || device>127)
  throw std::invalid_argument("Unsupported original Akai SDS format, size, ID or rate");
 const auto period=midi21(std::uint32_t((1'000'000'000ull+rate/2u)/rate));
 const auto length=midi21(std::uint32_t(mono.size()));
 const std::size_t packets=(mono.size()+39u)/40u;
 Dump out;
 out.frames=std::uint32_t(mono.size());out.rate=rate;out.sampleNumber=number;
 const std::size_t bytes=21u+127u*packets;
 // MAME 0.288 native MIDI image uses 100ms AFTER every sysex packet,
 // plus true 31250bit/s serial duration. 1s owner-thread safety margin.
 out.transportMillis=(std::uint64_t(bytes)*10u*1000u+31249u)/31250u
                     +std::uint64_t(packets+1u)*100u+1000u;
 out.sysex.reserve(bytes);
 auto put=[&](std::uint8_t b){out.sysex.push_back(b);};
 put(0xf0);put(0x7e);put(device);put(1);
 put(std::uint8_t(number&127));put(std::uint8_t((number>>7)&127));put(16);
 for(auto b:period)put(b);
 for(int i=0;i<3;++i)for(auto b:length)put(b); // length, loop start/end
 put(0x7f);put(0xf7); // no loop
 if(out.sysex.size()!=21)throw std::logic_error("SDS header error");
 for(std::size_t p=0;p<packets;++p){
  put(0xf0);put(0x7e);put(device);put(2);put(std::uint8_t(p&127));
  std::uint8_t checksum=std::uint8_t(0x7e^device^2^std::uint8_t(p&127));
  for(std::size_t i=0;i<40;++i){
   const auto at=p*40+i;
   const auto pcm=at<mono.size()?mono[at]:std::int16_t(-32768);
   // MIDI SDS 16-bit samples use unsigned biased PCM with high 7 bits first.
   const auto v=std::uint16_t(std::uint16_t(pcm)^0x8000u);
   const std::array<std::uint8_t,3> encoded={
     std::uint8_t((v>>9)&127),std::uint8_t((v>>2)&127),
     std::uint8_t((v&3)<<5)};
   for(auto b:encoded){put(b);checksum^=b;}
  }
  put(checksum&127);put(0xf7);
 }
 if(out.sysex.size()!=bytes)throw std::logic_error("SDS payload error");
 return out;
}
// Explicitly MONO conversion: MIDI SDS is one Akai mono sample.
// Stereo requires TWO samples and two separately panned original zones.
inline Dump encodeStereoAsMono(std::span<const std::int16_t> stereo,
                              std::uint32_t rate,std::uint16_t number=99,
                              std::uint8_t device=0){
 if(stereo.empty() || (stereo.size()%2) || stereo.size()/2>maxFrames)
  throw std::invalid_argument("Stereo PCM16 must be bounded and interleaved");
 std::vector<std::int16_t> mono;
 mono.reserve(stereo.size()/2);
 for(std::size_t i=0;i<stereo.size();i+=2){
  const auto sum=std::int32_t(stereo[i])+std::int32_t(stereo[i+1]);
  mono.push_back(std::int16_t(sum/2));
 }
 return encodeMono(mono,rate,number,device);
}
}
