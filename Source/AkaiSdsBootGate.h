// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// P035: Original Akai S3000XL firmware boot gate for real MIDI SDS imports.
// A powered emulated motherboard is NOT the same as original firmware READY.
// Only read the genuine current HD61830 LCD; original HDD wait requires F8 SKIP.
#include "AkaiCaptureAutomation.h"
#include <cstdint>
#include <string_view>
namespace akai::sds {
enum class BootPhase:std::uint8_t {Idle,WaitBoot,HoldF8,Ready,Failed};
enum class BootAction:std::uint8_t {None,PressF8,ReleaseF8};
struct BootDecision {
 BootPhase phase=BootPhase::Idle;
 BootAction action=BootAction::None;
};
class BootGate {
 BootPhase phase_=BootPhase::Idle;
 std::uint64_t entered_=0,pressed_=0,prior_=0;
 std::uint8_t skips_=0;
 public:
 BootPhase phase() const noexcept{return phase_;}
 bool ready()const noexcept{return phase_==BootPhase::Ready;}
 bool active()const noexcept{return phase_==BootPhase::WaitBoot || phase_==BootPhase::HoldF8;}
 bool start(std::uint64_t now)noexcept {
  if(active())return false;
  phase_=BootPhase::WaitBoot;entered_=now;pressed_=0;prior_=0;skips_=0;
  return true;
 }
 BootDecision fail()noexcept {
  const bool held=phase_==BootPhase::HoldF8;
  phase_=BootPhase::Failed;
  return {phase_,held?BootAction::ReleaseF8:BootAction::None};
 }
 BootDecision step(std::string_view lcd,bool readable,bool powered,
                   std::uint64_t now)noexcept {
  if(!active())return {phase_};
  if(!powered || now<entered_ || now-entered_>120000)return fail();
  if(phase_==BootPhase::HoldF8) {
   if(now<pressed_ || now-pressed_<500)return {phase_};
   phase_=BootPhase::WaitBoot;prior_=now;
   return {phase_,BootAction::ReleaseF8};
  }
  if(!readable || lcd.size()!=320)return {phase_}; // never press blindly
  if(akai::capture::originalSelectProgramPage(lcd)
     || akai::capture::originalSampleZonePage(lcd)
     || akai::capture::contains(lcd,"RECORD SET-UP")){
   phase_=BootPhase::Ready;return {phase_};
  }
  if(akai::capture::hasOriginalBootSkip(lcd)) {
   if(prior_ && now-prior_<2500)return {phase_};
   if(++skips_>8)return fail();
   phase_=BootPhase::HoldF8;pressed_=now;
   return {phase_,BootAction::PressF8};
  }
  // OS flash screen, disk-read and unknown screens are NOT proof of
  // firmware readiness. Neither F8 nor SDS is permitted there.
  return {phase_};
 }
};
}
