// P016: entirely independent SDS byte-contract tests; NO original firmware.
#include "../Source/AkaiMidiSds.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <stdexcept>
using namespace akai::sds;
static int checks=0;
static void check(bool v,const char* name){
    ++checks;if(!v){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}
}
template<class F>void rejects(F f,const char* name){
    try{f();check(false,name);}catch(const std::invalid_argument&){check(true,name);}
}
int main(){
    const auto h=makeHeader(0,1,44100,44100);
    check(h.size()==21&&h[0]==0xf0&&h[1]==0x7e&&h[3]==1&&h[20]==0xf7,"header");
    check(h[6]==16&&h[19]==0x7f,"sample16_no_loop");
    check(h[7]+(h[8]<<7)+(h[9]<<14)==22676,"44100_period");
    check(h[10]+(h[11]<<7)+(h[12]<<14)==44100,"44100_frames");
    rejects([]{(void)makeHeader(0,1,44100,0);},"zero_frames");
    rejects([]{(void)makeHeader(128,1,44100,40);},"invalid_device");
    rejects([]{(void)makeHeader(0,16384,44100,40);},"invalid_slot");
    rejects([]{(void)makeHeader(0,0,44100,maxSampleFrames+1);},"oversized_frames");
    std::vector<std::int16_t> pcm(81);
    pcm[0]=-32768;pcm[1]=0;pcm[2]=32767;pcm[3]=std::int16_t(0x70f0);
    const auto p=makePacket(0,0,pcm);
    check(p[0]==0xf0&&p[1]==0x7e&&p[3]==2&&p[4]==0&&p[126]==0xf7,"packet");
    check(p[5]==0&&p[6]==0&&p[7]==0,"pcm_negative");
    check(p[8]==0x40&&p[9]==0&&p[10]==0,"pcm_zero");
    check(p[11]==0x7f&&p[12]==0x7f&&p[13]==0x60,"pcm_positive");
    check(p[14]==0x78&&p[15]==0x3c&&p[16]==0,"sds_known_vector");
    unsigned sum=0x7e^2;for(int i=5;i<125;i++)sum^=p[i];
    check(p[125]==(sum&0x7f),"xor_checksum");
    const auto tail=makePacket(0,2,pcm);
    check(tail[4]==2&&tail[5]==0x40&&tail[6]==0&&tail[7]==0,"tail_sample");
    check(tail[8]==0&&tail[9]==0&&tail[10]==0,"tail_zero_padding");
    rejects([&]{(void)makePacket(0,3,pcm);},"out_of_bounds_packet");
    std::array<std::uint8_t,6> msg{0xf0,0x7e,5,0x7f,0,0xf7};
    check(parseReply(msg,5,0)==Reply::Ack,"ack");
    msg[3]=0x7e;check(parseReply(msg,5,0)==Reply::Nak,"nak");
    msg[3]=0x7c;check(parseReply(msg,5,0)==Reply::Wait,"wait");
    msg[3]=0x7d;check(parseReply(msg,5,0)==Reply::Cancel,"cancel");
    check(parseReply(msg,3,0)==Reply::Invalid,"wrong_device");
    check(parseReply(msg,5,1)==Reply::Invalid,"stale_packet");
    msg[0]=0;check(parseReply(msg,5,0)==Reply::Invalid,"bad_sysex");
    Sender s;check(s.begin(81),"begin");
    check(!s.begin(1),"no_reentrant_begin");
    check(s.initial()==Sender::Action::SendHeader,"send_header");
    check(s.onResponse(Reply::Nak)==Sender::Action::SendHeader,"retry_header");
    check(s.onResponse(Reply::Ack)==Sender::Action::SendPacket&&s.currentPacket()==0,"accept_header");
    check(s.onResponse(Reply::Wait)==Sender::Action::None,"pause");
    check(s.onResponse(Reply::Ack)==Sender::Action::SendPacket&&s.currentPacket()==1,"resume");
    check(s.onResponse(Reply::Nak)==Sender::Action::SendPacket&&s.currentPacket()==1,"retry_packet");
    check(s.onResponse(Reply::Ack)==Sender::Action::SendPacket&&s.currentPacket()==2,"third_packet");
    check(s.onResponse(Reply::Ack)==Sender::Action::Completed,"completed_after_last_ack");
    check(s.phase()==Sender::Phase::Completed,"completed_latched");
    Sender bad;check(bad.begin(40),"abort_begin");
    for(int i=0;i<3;i++)check(bad.onResponse(Reply::Nak)==Sender::Action::SendHeader,"bounded_retry");
    check(bad.onResponse(Reply::Nak)==Sender::Action::Aborted,"max_retries_fail_closed");
    std::printf("AKAI_SDS_CODEC=PASS checks=%d\n",checks);
    return 0;
}
