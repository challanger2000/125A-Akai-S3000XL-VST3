#include "../Source/AkaiMachineState.h"
#include <cstdlib>
#include <cstdio>
using namespace akai::machine_state;
static int tests=0;
static void ck(bool v,const char* name){
    ++tests;
    if(!v){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}
}
int main(){
    std::vector<std::uint8_t> payload(32768,0);
    std::memcpy(payload.data(),"MAMESAVE",8);
    for(std::size_t i=32;i<payload.size();++i)payload[i]=std::uint8_t(i*97u+5u);
    std::vector<std::uint8_t> envelope,decoded{55};
    ck(encode(payload,envelope),"accept_mame_magic");
    ck(envelope.size()==headerBytes+payload.size(),"length_exact");
    ck(decode(envelope,decoded)&&decoded==payload,"roundtrip_full_binary");
    auto failed=decoded;
    auto bad=envelope;bad.back()^=1;
    ck(!decode(bad,decoded)&&decoded==failed,"checksum_corruption_atomic");
    bad=envelope;bad[0]^=1;
    ck(!decode(bad,decoded)&&decoded==failed,"bad_magic_atomic");
    bad=envelope;bad[8]=2;
    ck(!decode(bad,decoded)&&decoded==failed,"bad_version_atomic");
    bad=envelope;bad[24]=1;
    ck(!decode(bad,decoded)&&decoded==failed,"nonzero_flags_atomic");
    bad=envelope;bad[12]^=1;
    ck(!decode(bad,decoded)&&decoded==failed,"bad_length_atomic");
    bad=envelope;bad[headerBytes]^=1;
    ck(!decode(bad,decoded)&&decoded==failed,"bad_machine_header_atomic");
    bad=envelope;bad.push_back(0);
    ck(!decode(bad,decoded)&&decoded==failed,"trailing_bytes_rejected");
    ck(!decode(std::span<const std::uint8_t>(envelope.data(),envelope.size()-1),decoded)
        &&decoded==failed,"truncated_payload_atomic");
    ck(!decode(std::span<const std::uint8_t>{},decoded)&&decoded==failed,"empty_rejected");
    std::vector<std::uint8_t> empty,keep{5,6};
    ck(!encode(empty,keep)&&keep==std::vector<std::uint8_t>({5,6}),
       "zero_payload_no_mutation");
    bad=payload;bad[0]^=1;
    ck(!encode(bad,keep)&&keep==std::vector<std::uint8_t>({5,6}),
       "bad_save_magic_no_mutation");
    payload.resize(32);
    ck(encode(payload,envelope),"minimal_32byte_mame_save");
    ck(decode(envelope,decoded)&&decoded==payload,"minimal_save_replaces_previous");
    // P020: project A restore may finish after project B has superseded it.
    RestoreMailbox mb;
    auto first=std::make_shared<const std::vector<std::uint8_t>>(32,11);
    auto second=std::make_shared<const std::vector<std::uint8_t>>(32,22);
    mb.publish(first);
    auto workA=mb.take();
    ck(bool(workA)&&workA.bytes==first&&mb.needsService(),
       "restore_mailbox_first_request");
    ck(mb.unresolved()==first,"resave_pending_original_image");
    mb.publish(second);
    mb.finish(workA,true);
    auto workB=mb.take();
    ck(bool(workB)&&workB.bytes==second && mb.needsService() &&
       mb.unresolved()==second,"stale_restore_completion_cannot_cancel_newer");
    mb.finish(workB,true);
    ck(!mb.take()&&!mb.needsService()&&!mb.unresolved(),
       "successful_restore_no_longer_pending");
    ck(mb.current()==second,"loaded_state_retained_for_stopped_host");
    mb.publish(first);
    auto failWork=mb.take();
    mb.finish(failWork,false);
    ck(mb.lastAttemptFailed() && !mb.needsService() &&
       !mb.take() && mb.unresolved()==first,
       "failed_io_preserves_image_without_unbounded_retry");
    mb.publish(second);
    ck(!mb.lastAttemptFailed()&&bool(mb.take())&&mb.needsService(),
       "new_project_after_failed_restore_rearms");
    mb.publish({});
    ck(!mb.take()&&!mb.unresolved()&&!mb.current()&&!mb.needsService(),
       "legacy_project_cancels_previous_native_restore");
    std::printf("AKAI_MACHINE_STATE_ENVELOPE_PASS %d\n",tests);
}
