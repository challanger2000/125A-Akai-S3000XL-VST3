// P015: original Akai firmware STOP/staged WAV adversarial contracts.
#include "../Source/AkaiWavCaptureState.h"
#include <cstdio>
#include <cstdlib>
using namespace akai::wav_capture;
static int n=0;
void ck(bool ok,const char* name){++n;if(!ok){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}}
int main(){
    ck(afterFirmwareStop(State::Recording,false,true)==State::Armed,"early_stop_not_complete");
    ck(afterFirmwareStop(State::Recording,true,true)==State::SourceComplete,"feed_finished_retained");
    ck(afterFirmwareStop(State::SourceComplete,false,true)==State::SourceComplete,"stop_latch_retained");
    ck(afterFirmwareStop(State::SourceComplete,true,false)==State::None,"unload_clears_completion");
    ck(canStage(State::None,false,false,false,false),"initial_import");
    ck(canStage(State::Armed,false,false,false,false),"idle_replace");
    ck(canStage(State::SourceComplete,false,false,false,false),"finished_restage");
    ck(!canStage(State::Recording,false,false,false,false),"midstream_restage_rejected");
    ck(!canStage(State::Armed,true,false,false,false),"firmware_go_restage_rejected");
    ck(!canStage(State::Armed,false,true,false,false),"automation_restage_rejected");
    ck(!canStage(State::Armed,false,false,true,false),"pending_slot_restage_rejected");
    ck(!canStage(State::Armed,false,false,false,true),"pending_clear_restage_rejected");
    std::printf("AKAI_WAV_CAPTURE_LIFECYCLE=PASS checks=%d\n",n);
    return 0;
}
