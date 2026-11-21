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
// P032: Convert one authentic SDS dump into a Standard MIDI File.
// MAME's native .syx parser inserts 100ms silence after each SysEx packet.
// The real owner-ROM accepts 42 MIDI ticks between packets: 43.75ms at
// 500000us/quarter, safely above the 40.64ms wire time for 127 bytes.
// Crucially this prevents MAME midiin's 131072-byte TX ring from flooding
// for ordinary WAVs that would exceed it if all packets shared tick zero.
inline std::vector<std::uint8_t> toTimedMidiFile(const Dump& dump) {
 const auto expected=21u+127u*dump.packets();
 if(dump.frames==0 || dump.sysex.size()!=expected)
  throw std::invalid_argument("Incomplete original Akai SDS dump");
 std::vector<std::uint8_t> track;
 track.reserve(expected+5u*dump.packets()+12u);
 const auto tempo=std::array<std::uint8_t,7>{0,0xff,0x51,0x03,0x07,0xa1,0x20};
 track.insert(track.end(),tempo.begin(),tempo.end());
 const auto append=[&](std::uint8_t b){track.push_back(b);};
 const auto length=[&](std::uint32_t n){
  std::array<std::uint8_t,5> digits{};
  int cnt=0;
  digits[size_t(cnt++)]=std::uint8_t(n&127);
  for(n>>=7;n;n>>=7)digits[size_t(cnt++)]=std::uint8_t((n&127)|128);
  while(cnt)append(digits[size_t(--cnt)]);
 };
 std::size_t offset=0;
 for(std::size_t i=0;i<=dump.packets();++i){
  const auto bytes=std::size_t(i==0?21:127);
  if(offset+bytes>dump.sysex.size() ||
     dump.sysex[offset]!=0xf0 || dump.sysex[offset+bytes-1]!=0xf7)
   throw std::invalid_argument("Malformed original Akai SDS SysEx boundary");
  length(std::uint32_t(i==0?0:42)); // MIDI ticks, not milliseconds
  append(0xf0);
  length(std::uint32_t(bytes-1)); // includes trailing F7 per SMF SysEx
  track.insert(track.end(),dump.sysex.begin()+std::ptrdiff_t(offset+1),
               dump.sysex.begin()+std::ptrdiff_t(offset+bytes));
  offset+=bytes;
 }
 track.insert(track.end(),{0,0xff,0x2f,0}); // End Of Track
 if(track.size()>0xffffffffull)
  throw std::invalid_argument("MIDI Sample Dump file too large");
 std::vector<std::uint8_t> out={
   'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
   'M','T','r','k'
 };
 const auto n=std::uint32_t(track.size());
 for(int shift=24;shift>=0;shift-=8)out.push_back(std::uint8_t(n>>shift));
 out.insert(out.end(),track.begin(),track.end());
 return out;
}
inline std::uint64_t timedMidiTransferMillis(const Dump& dump)noexcept {
 // Each packet occupies 42 ticks at 480 PPQN and 500000us/QN.
 // Approximate emulated time only. Never substitutes for Akai LCD proof.
 return (std::uint64_t(dump.packets())*42u*500u+479u)/480u
       +1500u;
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
