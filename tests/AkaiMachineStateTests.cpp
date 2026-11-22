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
    payload[8]=2; // MAME 0.288 format version
    std::memcpy(payload.data()+10,"s3000xl",8);
    payload[28]=0x49;payload[29]=0xa9;payload[30]=0x38;payload[31]=0x0e;
    ck(hasMameHeader(payload),"real_akai_v2_mame_header_accept");
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
    bad=envelope;bad[headerBytes+8]^=1;
    ck(!decode(bad,decoded)&&decoded==failed,
       "wrong_mame_save_format_version_rejected_atomic");
    bad=envelope;bad[headerBytes+10]='x';
    ck(!decode(bad,decoded)&&decoded==failed,
       "different_sampler_model_rejected_atomic");
    bad=envelope;bad[headerBytes+9]=0x80;
    ck(!decode(bad,decoded)&&decoded==failed,
       "unsupported_mame_header_flags_rejected_atomic");
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
    ck(mayStartSnapshot(false,false),"save_when_mame_idle");
    ck(!mayStartSnapshot(true,false),"pending_mame_load_blocks_new_save");
    ck(!mayStartSnapshot(false,true),"inflight_original_load_blocks_save");
    ck(!mayStartSnapshot(true,true),"both_inflight_blocks_save");
    ck(!rejectIncompleteProject(false,false,false),
       "legacy_project_without_marker_allowed");
    ck(!rejectIncompleteProject(false,false,true),
       "historical_native_without_completion_marker_allowed");
    ck(!rejectIncompleteProject(true,true,true),
       "complete_project_with_machine_image_allowed");
    ck(rejectIncompleteProject(true,false,false),
       "timed_out_host_save_must_fail_closed");
    ck(rejectIncompleteProject(true,true,false),
       "tampered_complete_flag_without_image_rejected");
    ck(rejectIncompleteProject(true,false,true),
       "failed_complete_flag_with_image_rejected");
    SnapshotBarrier snap;
    ck(!snap.begin(0,false,false)&&!snap.active(),
       "zero_generation_must_not_start");
    ck(!snap.begin(1,true,false)&&!snap.active(),
       "scheduler_pending_blocks_snapshot_start");
    ck(!snap.begin(1,false,true)&&!snap.active(),
       "restore_pending_blocks_snapshot_start");
    ck(snap.begin(17,false,false)&&snap.active()&&snap.sequence()==17,
       "begin_snapshot_with_generation");
    ck(!snap.ready(true)&&snap.active(),"deferred_save_must_not_ack_early");
    ck(!snap.begin(18,false,false)&&snap.sequence()==17,
       "second_generation_must_not_replace_inflight_save");
    ck(snap.ready(false)&&snap.sequence()==17,
       "ack_only_when_mame_saveload_cleared");
    snap.clear();
    ck(!snap.ready(false)&&!snap.active(),
       "cleared_snapshot_cannot_respond_twice");
    ck(snap.begin(18,false,false)&&snap.sequence()==18&&snap.ready(false),
       "subsequent_generation_after_finish");
    snap.clear();
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
