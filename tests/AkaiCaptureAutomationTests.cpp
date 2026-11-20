// Native P004 Akai original-firmware guarded automation contract.
// Synthetic LCD/WADCS observations: not a replacement for a ROM-backed test.
#include "../Source/AkaiCaptureAutomation.h"
#include <cstdio>
#include <cstdlib>
using namespace akai::capture;
static int n=0;
static void ck(bool ok,const char* id){++n;if(!ok){std::fprintf(stderr,"FAIL %s\n",id);std::exit(1);}}
static Observe at(std::string_view lcd,bool wav=true,std::uint8_t latch=0xa2,
                  bool complete=false) {
    Observe o; o.lcd=lcd;o.readable=true;o.firmwareReady=true;
    o.wavReady=wav;o.wadcsKnown=true;o.wadcs=latch;o.wavComplete=complete;return o;
}
int main(){
    ck(contains("Record Set-Up","RECORD SET-UP"),"case_insensitive");
    ck(!contains("RECORD","RECORD SET-UP"),"whole_required");
    ck(overwriteWarning("overwrite existing sample!?"),"overwrite");
    ck(!overwriteWarning("record time: 01:22"),"ordinary_record");
    Controller c;
    ck(c.start(0),"begin");
    auto r=c.step(at("RECORD SET-UP"),0);
    ck(r.command==Command::Press&&r.button==Button::RecF2&&
       r.stage==Stage::PressRec,"press_rec_only_after_setup");
    ck(!c.start(20),"cannot_double_start");
    r=c.step(at("RECORD SET-UP"),50);
    ck(r.command==Command::None,"key_held_for_scan");
    r=c.step(at("RECORD SET-UP"),121);
    ck(r.command==Command::Release&&r.button==Button::RecF2&&
       r.stage==Stage::WaitNewSample,"release_rec");
    r=c.step(at("RECORD TIME 01s",true),150);
    ck(r.command==Command::None,"no_arm_without_new_name");
    r=c.step(at("sample: BASS001 - new -",true),200);
    ck(r.command==Command::Press&&r.button==Button::ArmF8,"arm_new_only");
    r=c.step(at("sample: BASS001 - new -"),330);
    ck(r.command==Command::Release,"release_arm");
    r=c.step(at("recording setup",true,0x82),400);
    ck(r.command==Command::None,"no_go_until_waiting_for_start");
    r=c.step(at("WAITING FOR START ...... GO EXIT",true,0x82),500);
    ck(r.command==Command::Press && r.button==Button::GoF7,"go_from_verified_arm");
    r=c.step(at("WAITING FOR START",true,0x82),621);
    ck(r.command==Command::Release,"release_go");
    r=c.step(at("RECORDING analog",true,0x8a),750);
    ck(r.stage==Stage::Capturing,"record_strobe_observed");
    r=c.step(at("RECORDING analog",true,0x8a,true),980);
    ck(r.stage==Stage::WaitStop,"wav_exhausted_wait_native_stop");
    r=c.step(at("RECORDING analog",true,0xa2,true),1150);
    ck(r.stage==Stage::RecordedSample,"firmware_stop_seen");
    ck(!c.isRunning(),"finished_and_stopped");
    Controller nav;ck(nav.start(0),"second_start");
    r=nav.step(at("SINGLE PROGRAM MODE"),5);
    ck(r.command==Command::Press&&r.button==Button::Sample,"navigate_to_sample");
    r=nav.step(at("SINGLE PROGRAM MODE"),130);
    ck(r.command==Command::Release,"release_sample");
    r=nav.step(at("RECORD SET-UP"),200);
    ck(r.command==Command::Press&&r.button==Button::RecF2,"rec_after_navigation");
    Controller safety;ck(safety.start(0),"safety_start");
    r=safety.step(at("OVERWRITE EXISTING SAMPLE!?"),1);
    ck(r.stage==Stage::OverwriteRisk&&r.command==Command::None,"never_confirm_overwrite");
    Controller unique;ck(unique.start(0),"unique_start");
    unique.step(at("RECORD SET-UP"),0);
    unique.step(at("RECORD SET-UP"),140);
    r=unique.step(at("EXISTING NAME",true),9000);
    ck(r.stage==Stage::NeedNewSample,"fail_closed_no_unique_name");
    Controller noWav;ck(noWav.start(0),"nowav_start");
    r=noWav.step(at("RECORD SET-UP",false),200);
    ck(r.command==Command::None,"never_press_without_wav");
    Controller cancelled;ck(cancelled.start(0),"cancel_start");
    cancelled.step(at("RECORD SET-UP"),0);
    r=cancelled.cancel(50);
    ck(r.stage==Stage::Cancelled&&r.command==Command::Release&&
       r.button==Button::RecF2,"cancel_releases_only_owned_key");
    ck(!cancelled.isRunning(),"cancelled_stops");
    Controller unknown;ck(unknown.start(0),"unknown_start");
    Observe noDisplay=at("",true);noDisplay.readable=false;
    r=unknown.step(noDisplay,100);
    ck(r.command==Command::None,"no_blind_buttons");
    r=unknown.step(noDisplay,31000);
    ck(r.stage==Stage::TimedOut,"no_display_timeout");
    std::printf("AKAI_FIRMWARE_CAPTURE_CONTROLLER=PASS checks=%d\n",n);
    return 0;
}
