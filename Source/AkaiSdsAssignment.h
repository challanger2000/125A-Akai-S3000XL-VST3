// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Native original S3000XL firmware SDS post-transfer program assignment guard.
// Read the ORIGINAL HD61830 40x8 text LCD; do not read or write guest memory.
#include "AkaiCaptureAutomation.h"
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace akai::sds {
inline bool validOriginalZoneOne(std::string_view lcd,
                                 std::string_view sample="MIDI 99") noexcept {
 if(lcd.size()!=320 || sample.empty() ||
    !akai::capture::originalSampleZonePage(lcd))return false;
 for(std::size_t row=0;row<8;++row) {
  const auto line=lcd.substr(row*40,40);
  if(line[0]!='1')continue;
  const auto at=line.find(sample);
  if(at==std::string_view::npos || at<2 || at>9)continue;
  const auto end=at+sample.size();
  // Original Akai velocity columns: a zone's "?" flags non-existent
  // samples; zone 2..4 are legitimately empty and MUST NOT be scanned.
  if(end>=40 || line.find('?',at)!=std::string_view::npos
      && line.find('?',at)<line.find("127",at))return false;
  if(line.find("127",end)==std::string_view::npos)return false;
  return true;
 }
 return false;
}
enum class Phase:std::uint8_t {
 Idle, PressSingle, WaitSingle, PressEdit, WaitEdit,
 PressKeygroups, WaitKeygroups, PressSample, WaitSample,
 Dial, WaitDial, Verified, Failed
};
enum class Action:std::uint8_t {None,Down,Up,DialIncrement};
enum class Button:std::uint8_t {None,Single,Edit,F2,F6};
struct Decision {
 Phase stage=Phase::Idle;
 Action action=Action::None;
 Button button=Button::None;
};
class Assignment {
 Phase phase_=Phase::Idle;
 Button held_=Button::None;
 std::uint64_t entered_=0;
 int increments_=0;
 public:
 bool start(std::uint64_t now)noexcept {
  if(running())return false;
  phase_=Phase::PressSingle;held_=Button::None;
  entered_=now;increments_=0;return true;
 }
 bool running()const noexcept {
  return phase_!=Phase::Idle && phase_!=Phase::Verified && phase_!=Phase::Failed;
 }
 bool verified()const noexcept{return phase_==Phase::Verified;}
 Phase phase()const noexcept{return phase_;}
 Decision fail()noexcept {
  const auto held=held_;
  held_=Button::None;phase_=Phase::Failed;
  return {phase_,held==Button::None?Action::None:Action::Up,held};
 }
 Decision step(std::string_view lcd,bool readable,
               bool firmwareReady,std::uint64_t now)noexcept {
  if(!running())return {phase_};
  if(!firmwareReady || !readable ||
     now<entered_ || now-entered_>9000)return fail();
  if(held_!=Button::None) {
   if(now-entered_<350)return {phase_};
   const auto b=held_;held_=Button::None;
   switch(phase_) {
   case Phase::PressSingle:phase_=Phase::WaitSingle;break;
   case Phase::PressEdit:phase_=Phase::WaitEdit;break;
   case Phase::PressKeygroups:phase_=Phase::WaitKeygroups;break;
   case Phase::PressSample:phase_=Phase::WaitSample;break;
   default:return fail();
   }
   entered_=now;
   return {phase_,Action::Up,b};
  }
  auto press=[&](Phase stage,Button key)->Decision{
   phase_=stage;held_=key;entered_=now;
   return {phase_,Action::Down,key};
  };
  switch(phase_) {
  case Phase::PressSingle:return press(phase_,Button::Single);
  case Phase::WaitSingle:
   if(akai::capture::originalSelectProgramPage(lcd))
    return press(Phase::PressEdit,Button::Edit);
   break;
  case Phase::WaitEdit:
   if(akai::capture::originalProgramEditPage(lcd))
    return press(Phase::PressKeygroups,Button::F2);
   break;
  case Phase::WaitKeygroups:
   if(akai::capture::originalKeygroupsPage(lcd))
    return press(Phase::PressSample,Button::F6);
   break;
  case Phase::WaitSample:
  case Phase::WaitDial:
   if(validOriginalZoneOne(lcd)) {
    phase_=Phase::Verified;
    return {phase_};
   }
   if(akai::capture::originalSampleZonePage(lcd)) {
    // Protect the original TEST PROGRAM against arbitrary replacement.
    // First zone starts at SINE; owner V2.0 original test required two
    // clockwise DATAENTRY encoder ticks to reach the received MIDI 99.
    if(increments_>=4)return fail();
    if(phase_==Phase::WaitDial && now-entered_<650)break;
    if(increments_==0 && !akai::capture::contains(lcd,"SINE"))
     return fail();
    ++increments_;phase_=Phase::WaitDial;entered_=now;
    return {phase_,Action::DialIncrement};
   }
   break;
  default:break;
  }
  return {phase_};
 }
};
}
