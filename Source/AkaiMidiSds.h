// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// P016 research-only native MIDI SDS: never active in VST3 until ROM-backed acceptance.
// Original MIDI Sample Dump Standard. 16-bit mono, 7-bit SysEx packets.
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
namespace akai::sds {
inline constexpr std::uint32_t maxSampleFrames=0x1fffffu;
inline constexpr unsigned framesPerPacket=40;
using Header=std::array<std::uint8_t,21>;
using Packet=std::array<std::uint8_t,127>;
enum class Reply:unsigned char {Invalid,Ack,Nak,Wait,Cancel};
inline void put21(std::uint8_t* p,std::uint32_t v){
    if(v>maxSampleFrames)throw std::invalid_argument("SDS length exceeds 21 bits");
    p[0]=std::uint8_t(v&0x7f);p[1]=std::uint8_t((v>>7)&0x7f);
    p[2]=std::uint8_t((v>>14)&0x7f);
}
inline Header makeHeader(std::uint8_t device,std::uint16_t number,
                         std::uint32_t rate,std::uint32_t frames){
    if(device>127||number>16383||rate<8000||rate>192000||
       frames==0||frames>maxSampleFrames)
        throw std::invalid_argument("Invalid SDS header parameters");
    Header h{};
    h[0]=0xf0;h[1]=0x7e;h[2]=device;h[3]=0x01;
    h[4]=std::uint8_t(number&0x7f);h[5]=std::uint8_t((number>>7)&0x7f);
    h[6]=16;
    put21(h.data()+7,std::uint32_t((1'000'000'000ull+rate/2)/rate));
    put21(h.data()+10,frames);
    put21(h.data()+13,frames);put21(h.data()+16,frames);
    h[19]=0x7f;h[20]=0xf7;return h;
}
inline Packet makePacket(std::uint8_t device,std::uint32_t index,
                         std::span<const std::int16_t> pcm){
    if(device>127||pcm.empty()||pcm.size()>maxSampleFrames||
       index>=(pcm.size()+framesPerPacket-1)/framesPerPacket)
        throw std::invalid_argument("Invalid SDS packet parameters");
    Packet p{};p[0]=0xf0;p[1]=0x7e;p[2]=device;p[3]=0x02;
    p[4]=std::uint8_t(index&0x7f);
    const auto first=std::size_t(index)*framesPerPacket;
    for(std::size_t j=0;j<framesPerPacket && first+j<pcm.size();++j){
        // Original SDS16 transmits offset-binary words, not signed PCM.
        const auto word=std::uint16_t(std::int32_t(pcm[first+j])+32768);
        p[5+j*3]=std::uint8_t((word>>9)&0x7f);
        p[6+j*3]=std::uint8_t((word>>2)&0x7f);
        p[7+j*3]=std::uint8_t((word&3)<<5);
    }
    std::uint8_t sum=std::uint8_t(0x7e ^ device ^ 0x02 ^ p[4]);
    for(std::size_t j=5;j<125;++j)sum^=p[j];
    p[125]=sum&0x7f;p[126]=0xf7;return p;
}
inline Reply parseReply(std::span<const std::uint8_t> msg,std::uint8_t device,
                        std::uint8_t expectedPacket)noexcept{
    if(device>127||msg.size()!=6||msg[0]!=0xf0||msg[1]!=0x7e||
       msg[2]!=device||msg[4]!=expectedPacket||msg[5]!=0xf7)
        return Reply::Invalid;
    switch(msg[3]){
    case 0x7f:return Reply::Ack;
    case 0x7e:return Reply::Nak;
    case 0x7c:return Reply::Wait;
    case 0x7d:return Reply::Cancel;
    default:return Reply::Invalid;
    }
}
// Pure protocol state: transport pacing, hardware MIDI loopback, timeouts
// and original-ROM acceptance are NOT implemented or asserted here.
class Sender {
public:
    enum class Phase {Idle,Header,Packet,Paused,Completed,Aborted};
    enum class Action {None,SendHeader,SendPacket,Completed,Aborted};
    bool begin(std::uint32_t frames)noexcept{
        if(phase_!=Phase::Idle||frames==0||frames>maxSampleFrames)return false;
        count_=(frames+framesPerPacket-1)/framesPerPacket;
        index_=0;retries_=0;phase_=Phase::Header;return true;
    }
    Action initial()const noexcept{
        return phase_==Phase::Header?Action::SendHeader:Action::None;
    }
    Action onResponse(Reply r)noexcept{
        if(phase_!=Phase::Header&&phase_!=Phase::Packet&&phase_!=Phase::Paused)
            return Action::None;
        if(r==Reply::Invalid)return Action::None;
        if(r==Reply::Cancel){phase_=Phase::Aborted;return Action::Aborted;}
        if(r==Reply::Wait){resume_=phase_;phase_=Phase::Paused;return Action::None;}
        if(phase_==Phase::Paused)phase_=resume_;
        if(r==Reply::Nak){
            if(++retries_>3){phase_=Phase::Aborted;return Action::Aborted;}
            return phase_==Phase::Header?Action::SendHeader:Action::SendPacket;
        }
        retries_=0;
        if(phase_==Phase::Header){phase_=Phase::Packet;return Action::SendPacket;}
        if(++index_==count_){phase_=Phase::Completed;return Action::Completed;}
        return Action::SendPacket;
    }
    Phase phase()const noexcept{return phase_;}
    std::uint32_t currentPacket()const noexcept{return index_;}
    std::uint8_t expectedPacketByte()const noexcept{
        return phase_==Phase::Header?0:std::uint8_t(index_&0x7f);
    }
private:
    Phase phase_=Phase::Idle,resume_=Phase::Idle;
    std::uint32_t count_=0,index_=0;unsigned retries_=0;
};
} // namespace akai::sds
