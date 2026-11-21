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
    ck(hasUniqueRecordName("RECORD MONO * new * STRING C5 73%F"),"real_manual_star_new");
    ck(hasUniqueRecordName("RECORD MONO - new - STRING C5 73%F"),"legacy_dash_new");
    ck(!hasUniqueRecordName("SAMPLE EDIT * new * STRING C5"),"not_outside_record");
    ck(!hasUniqueRecordName("RECORD MONO STRING C5"),"not_existing_name");
    ck(!overwriteWarning("record time: 01:22"),"ordinary_record");
    // Cold-boot WAV import is queued until the ORIGINAL firmware is ready.
    // The previous unconditional !firmwareReady guard immediately discarded it.
    Controller booting;
    ck(booting.start(0),"cold_boot_import_start");
    Observe cold=at("RECORD SET-UP"); cold.firmwareReady=false;
    auto bootDecision=booting.step(cold,0);
    ck(bootDecision.command==Command::None &&
       bootDecision.stage==Stage::WaitWav,"cold_boot_no_early_abort");
    bootDecision=booting.step(cold,1000);
    ck(bootDecision.command==Command::None &&
       bootDecision.stage==Stage::WaitWav,"cold_boot_no_blind_panel_key");
    bootDecision=booting.step(at("RECORD SET-UP"),1100);
    ck(bootDecision.command==Command::Press &&
       bootDecision.button==Button::RecF2,"cold_boot_resumes_real_rec");
    Controller neverReady;
    ck(neverReady.start(0),"cold_boot_timeout_start");
    neverReady.step(cold,0);
    bootDecision=neverReady.step(cold,30001);
    ck(bootDecision.command==Command::None &&
       bootDecision.stage==Stage::TimedOut,"cold_boot_finite_timeout");
    Controller interrupted;
    ck(interrupted.start(0),"firmware_loss_start");
    interrupted.step(at("RECORD SET-UP"),0);
    bootDecision=interrupted.step(cold,50);
    ck(bootDecision.stage==Stage::TimedOut &&
       bootDecision.command==Command::Release &&
       bootDecision.button==Button::RecF2,
       "firmware_loss_releases_owned_key");

    {
    // P023 actual ORIGINAL ROM V2.0 headless LCD regression.
    // Owner's ROM boot was measured: at 4s "OPERATING SYSTEM 2.00",
    // at 9-23s "waiting for hard disk ready.. SKIP". A real F8 at
    // emulated 23.5s brought up SLCT/MIX and then SAMPLE→RECORD SET-UP.
    ck(isOriginalStartupScreen("PROGRAMS IN MEMORY OPERATING SYSTEM 2.00"),
       "original_startup_not_record_ready");
    ck(hasOriginalBootSkip("waiting for hard disk ready.. SKIP"),
       "real_rom_hdd_wait_skip_prompt");
    ck(!hasOriginalBootSkip("WAITING FOR HARD DISK READY"),
       "no_skip_label_no_blind_f8");
    Controller bootCapture;
    ck(bootCapture.start(0),"boot_hdd_capture_start");
    auto boot=at("PROGRAMS IN MEMORY OPERATING SYSTEM 2.00");
    auto r=bootCapture.step(boot,100);
    ck(r.stage==Stage::WaitWav && r.command==Command::None,
       "no_sample_during_original_rom_startup");
    r=bootCapture.step(at("PROGRAMS IN MEMORY waiting for hard disk ready.. SKIP"),300);
    ck(r.stage==Stage::PressBootSkip && r.command==Command::Press &&
       r.button==Button::ArmF8,"native_f8_skip_pending_hdd");
    r=bootCapture.step(at("waiting for hard disk ready.. SKIP"),600);
    ck(r.command==Command::None,"boot_hardware_key_500ms");
    r=bootCapture.step(at("waiting for hard disk ready.. SKIP"),801);
    ck(r.command==Command::Release && r.button==Button::ArmF8 &&
       r.stage==Stage::WaitBootReady,"release_boot_skip_on_mame_owner_thread");
    r=bootCapture.step(at("waiting for hard disk ready.. SKIP"),1200);
    ck(r.command==Command::None && r.stage==Stage::WaitBootReady,
       "await_actual_native_firmware_ready");
    r=bootCapture.step(at("waiting for hard disk ready.. SKIP"),3400);
    ck(r.command==Command::Press && r.button==Button::ArmF8,
       "retry_skip_only_after_firmware_timeout");
    r=bootCapture.step(at("waiting for hard disk ready.. SKIP"),3910);
    ck(r.command==Command::Release &&
       r.stage==Stage::WaitBootReady,"release_boot_skip_retry");
    r=bootCapture.step(at("PROGRAMS IN MEMORY SLCT MIX MIDI LOUD DISK DEL RNUM"),4000);
    ck(r.command==Command::None && r.stage==Stage::WaitWav,
       "exact_native_ready_page_after_f8");
    r=bootCapture.step(at("PROGRAMS IN MEMORY SLCT MIX MIDI LOUD DISK DEL RNUM"),4010);
    ck(r.command==Command::Press && r.button==Button::Sample,
       "sample_only_after_original_hdd_boot_skip");
    Controller noSkip;
    ck(noSkip.start(0),"no_skip_label_start");
    r=noSkip.step(at("WAITING FOR HARD DISK READY"),2000);
    ck(r.command==Command::None && r.stage==Stage::WaitWav,
       "never_guess_f8_if_original_lcd_has_no_skip");
    ck(isRunningStage(Stage::PressBootSkip) &&
       isRunningStage(Stage::WaitBootReady),
       "wav_replacement_locked_during_boot_skip");
    }
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
    r=c.step(at("RECORD MONO * new * BASS001",true),200);
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
    // A UI-side staged WAV removal cannot be ignored before original GO.
    Controller lost;ck(lost.start(0),"lost_begin");
    r=lost.step(at("RECORD SET-UP"),0);
    ck(r.command==Command::Press,"lost_press_rec");
    r=lost.step(at("RECORD SET-UP",false),50);
    ck(r.stage==Stage::SourceUnavailable && r.command==Command::Release &&
       r.button==Button::RecF2,"source_disappears_releases_held_rec_key");
    Controller shortRec;ck(shortRec.start(0),"short_record_begin");
    shortRec.step(at("RECORD SET-UP"),0);
    shortRec.step(at("RECORD SET-UP"),121);
    shortRec.step(at("RECORD MONO * new * BASS001"),200);
    shortRec.step(at("RECORD MONO * new * BASS001"),330);
    shortRec.step(at("WAITING FOR START",true,0x82),500);
    shortRec.step(at("WAITING FOR START",true,0x82),621);
    r=shortRec.step(at("RECORDING analog",true,0x8a),750);
    ck(r.stage==Stage::Capturing,"short_record_detect_native_go");
    r=shortRec.step(at("RECORD MONO",true,0xa2,false),800);
    ck(r.stage==Stage::SampleTruncated && !shortRec.isRunning(),
       "short_firmware_record_must_not_pass");
    Controller lostDuring;ck(lostDuring.start(0),"lost_midstream_begin");
    lostDuring.step(at("RECORD SET-UP"),0);
    lostDuring.step(at("RECORD SET-UP"),121);
    lostDuring.step(at("RECORD MONO * NEW *"),200);
    lostDuring.step(at("RECORD MONO * NEW *"),330);
    lostDuring.step(at("WAITING FOR START",true,0x82),500);
    lostDuring.step(at("WAITING FOR START",true,0x82),621);
    lostDuring.step(at("RECORDING",true,0x8a),750);
    auto missing=at("RECORDING",false,0x8a);missing.wavSourcePresent=false;
    r=lostDuring.step(missing,850);
    ck(r.stage==Stage::SourceUnavailable && !lostDuring.isRunning(),
       "lost_source_during_record_fails_closed");
    // P018 regression: a fresh original-firmware REC page usually has
    // an EXISTING sample name; it must be named through panel keys instead
    // of the old 8-second NeedNewSample timeout.
    Controller autoNamed;ck(autoNamed.start(0),"auto_name_start");
    r=autoNamed.step(at("RECORD SET-UP"),0);
    ck(r.button==Button::RecF2,"auto_name_setup_to_rec");
    r=autoNamed.step(at("RECORD SET-UP"),121);
    ck(r.command==Command::Release,"auto_name_rec_released");
    r=autoNamed.step(at("RECORD MONO SINE C3"),550);
    ck(r.button==Button::Name && r.stage==Stage::PressName,
       "auto_name_uses_original_name_button");
    const auto letters="LETTERS ..(NAME for numbers ENT to exit)";
    const auto numbers="NUMBERS ..(NAME for letters ENT to exit)";
    ck(lettersEditorVisible(letters)&&!lettersEditorVisible("RECORD MONO SINE"),
       "actual_original_rom_letters_header_required");
    ck(numbersEditorVisible(numbers)&&!numbersEditorVisible(letters),
       "actual_original_rom_numbers_header_required");
    r=autoNamed.step(at(letters),680);
    ck(r.command==Command::Release && r.button==Button::Name,
       "auto_name_release_first_name_key");
    r=autoNamed.step(at("RECORD MONO SINE"),690);
    ck(r.command==Command::None,
       "no_blind_name_toggle_on_unrelated_lcd_page");
    r=autoNamed.step(at(letters),720);
    ck(r.command==Command::Press && r.button==Button::Name,
       "auto_name_toggle_numeric_mode_only_in_original_letter_editor");
    r=autoNamed.step(at(letters),850);
    ck(r.command==Command::Release && r.stage==Stage::TypeName,
       "auto_name_numeric_ready");
    for(unsigned i=0;i<12;++i){
        auto atMs=1000+170*i;
        if(i==0) {
            r=autoNamed.step(at(letters),atMs);
            ck(r.command==Command::None,
               "no_blind_digit_on_alphabet_editor_or_unrelated_page");
        }
        r=autoNamed.step(at(numbers),atMs);
        ck(r.command==Command::Press && r.button==
            (i==0?Button::Digit1:i==1?Button::Digit2:
             i==2?Button::Digit5:i==11?Button::Digit1:Button::Digit0),
             "auto_name_correct_12_digit_sequence");
        r=autoNamed.step(at(numbers),atMs+121);
        ck(r.command==Command::Release,"auto_name_each_key_released");
    }
    r=autoNamed.step(at(numbers),3100);
    ck(r.command==Command::Press && r.button==Button::Enter,
       "auto_name_enter");
    r=autoNamed.step(at(numbers),3230);
    ck(r.command==Command::Release && r.stage==Stage::WaitNamed,
       "auto_name_enter_released");
    r=autoNamed.step(at("RECORD MONO * NEW * 125000000001"),3400);
    ck(r.command==Command::Press && r.button==Button::ArmF8,
       "auto_name_only_arm_after_real_firmware_unique_confirmation");
    // Disappearing source during name entry must cancel and release keys.
    Controller vanish;ck(vanish.start(0),"name_vanish_begin");
    vanish.step(at("RECORD SET-UP"),0);
    vanish.step(at("RECORD SET-UP"),121);
    vanish.step(at("RECORD MONO SINE"),540);
    r=vanish.step(at("NAMING",false),600);
    ck(r.stage==Stage::SourceUnavailable && r.button==Button::Name &&
       r.command==Command::Release,"name_source_lost_releases_button");
    ck(isRunningStage(Stage::TypeName)&&!isRunningStage(Stage::RecordedSample),
       "busy_stage_guard_knows_name_digits");
    Controller unknown;ck(unknown.start(0),"unknown_start");
    Observe noDisplay=at("",true);noDisplay.readable=false;
    r=unknown.step(noDisplay,100);
    ck(r.command==Command::None,"no_blind_buttons");
    r=unknown.step(noDisplay,31000);
    ck(r.stage==Stage::TimedOut,"no_display_timeout");
    std::printf("AKAI_FIRMWARE_CAPTURE_CONTROLLER=PASS checks=%d\n",n);
    return 0;
}
