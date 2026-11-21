// SPDX-License-Identifier: GPL-3.0-only
// P037: adversarial REAL WAV->original Akai MIDI Sample Dump pure pipeline.
// This exercises the same production source functions as editor import, not
// just a standalone SDS packet fixture. No ROMs, guest RAM or synth mocks.
#include "../Source/AkaiWavInput.h"
#include "../Source/AkaiMidiSampleDump.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <vector>

#define CHECK(x) do { if(!(x)) { \
    std::fprintf(stderr,"P037 WAV->SDS E2E failed at line %d: %s\n",__LINE__,#x); \
    return EXIT_FAILURE; } } while(false)

static void put16(std::vector<std::uint8_t>& out,std::uint16_t value) {
    out.push_back(std::uint8_t(value));out.push_back(std::uint8_t(value>>8));
}
static void put32(std::vector<std::uint8_t>& out,std::uint32_t value) {
    put16(out,std::uint16_t(value));put16(out,std::uint16_t(value>>16));
}
static void four(std::vector<std::uint8_t>& b,const char* s) {
    for(int i=0;i<4;++i)b.push_back(std::uint8_t(s[i]));
}
static std::vector<std::uint8_t> pcm16wav(
        std::span<const std::int16_t> samples,
        std::uint32_t rate,std::uint16_t channels) {
    std::vector<std::uint8_t> out;
    four(out,"RIFF");put32(out,std::uint32_t(samples.size()*2+36));
    four(out,"WAVE");four(out,"fmt ");put32(out,16);
    put16(out,1);put16(out,channels);put32(out,rate);
    put32(out,rate*channels*2);put16(out,channels*2);put16(out,16);
    four(out,"data");put32(out,std::uint32_t(samples.size()*2));
    for(auto n:samples)put16(out,std::uint16_t(n));
    return out;
}
static std::uint32_t readVlq(
        std::span<const std::uint8_t> data,std::size_t& pos) {
    std::uint32_t n=0;
    for(int i=0;i<5;++i) {
        if(pos>=data.size())throw std::runtime_error("Truncated MIDI VLQ");
        const auto v=data[pos++];
        n=(n<<7)|(v&127u);
        if(!(v&128u))return n;
    }
    throw std::runtime_error("Overlong MIDI VLQ");
}
// Independently parse the SMF output, recovering all MIDI SDS packets.
// Never use the encoder to validate its own MIDI packet framing.
static std::vector<std::uint8_t> unpackMidi(
        std::span<const std::uint8_t> midi,
        std::size_t expectedPackets) {
    if(midi.size()<35 ||
       std::memcmp(midi.data(),"MThd",4)!=0 ||
       std::memcmp(midi.data()+14,"MTrk",4)!=0 ||
       midi[8]!=0 || midi[9]!=0 || midi[10]!=0 || midi[11]!=1)
        throw std::runtime_error("Not standard format-0 MIDI");
    std::size_t p=22; // MThd 14 + MTrk 8
    std::vector<std::uint8_t> reconstructed;
    const auto time0=readVlq(midi,p);
    if(time0 || midi[p++]!=0xff || midi[p++]!=0x51 ||
       midi[p++]!=3 || midi[p++]!=7 || midi[p++]!=0xa1 ||
       midi[p++]!=0x20)throw std::runtime_error("No 120BPM tempo event");
    std::size_t lastNote=0;
    for(std::size_t packet=0;packet<=expectedPackets;++packet) {
        const auto delta=readVlq(midi,p);
        const auto expected=packet==0?0u:packet==1?10u:
            (packet-1u)%64u==0?2500u:0u;
        if(delta!=expected)throw std::runtime_error("Wrong grouped original UART pacing");
        if(midi[p++]!=0xf0)throw std::runtime_error("MIDI SDS SysEx event missing");
        const auto bytes=readVlq(midi,p);
        if(bytes!=(packet?126u:20u) || p+bytes>midi.size())
            throw std::runtime_error("MIDI SysEx length mismatch");
        reconstructed.push_back(0xf0);
        reconstructed.insert(reconstructed.end(),midi.begin()+std::ptrdiff_t(p),
                             midi.begin()+std::ptrdiff_t(p+bytes));
        p+=bytes;
        if(reconstructed.back()!=0xf7)
            throw std::runtime_error("MIDI SysEx F7 missing");
        lastNote+=delta;
    }
    if(p+4!=midi.size() || midi[p]!=0 || midi[p+1]!=0xff ||
       midi[p+2]!=0x2f || midi[p+3]!=0)
       throw std::runtime_error("MIDI EOT mismatch");
    return reconstructed;
}
static std::vector<std::int16_t> decodeSdsPcm(
        std::span<const std::uint8_t> sds,std::size_t frames) {
    if(sds.size()<21 || sds[0]!=0xf0 || sds[1]!=0x7e ||
       sds[2]!=0 || sds[3]!=1 || sds[4]!=99 || sds[6]!=16 ||
       sds[20]!=0xf7)
        throw std::runtime_error("Wrong genuine Akai SDS universal header");
    auto f=std::uint32_t(sds[10]) | (std::uint32_t(sds[11])<<7) |
           (std::uint32_t(sds[12])<<14);
    if(f!=frames)throw std::runtime_error("SDS length mismatch");
    std::vector<std::int16_t> decoded;
    for(std::size_t packet=0;packet<(frames+39)/40;++packet) {
        const auto pos=21+127*packet;
        if(pos+127>sds.size() || sds[pos]!=0xf0 ||
           sds[pos+1]!=0x7e || sds[pos+2]!=0 || sds[pos+3]!=2 ||
           sds[pos+4]!=(packet&127u) || sds[pos+126]!=0xf7)
           throw std::runtime_error("Original Akai SDS packet invalid");
        std::uint8_t check=0;
        for(std::size_t k=1;k<125;++k)check^=sds[pos+k];
        if(check!=sds[pos+125])
            throw std::runtime_error("SDS original XOR check invalid");
        for(std::size_t i=0;i<40;++i) {
            const auto at=pos+5+i*3;
            const std::uint16_t biased=std::uint16_t(
                (std::uint16_t(sds[at])<<9)
              | (std::uint16_t(sds[at+1])<<2)
              | (std::uint16_t(sds[at+2])>>5));
            decoded.push_back(std::int16_t(biased^0x8000u));
        }
    }
    decoded.resize(frames);
    return decoded;
}
int main() {
    // Test a real valid one-second user WAV, not a pre-encoded pseudo-MIDI
    // asset. 44.1kHz PCM16 mono is exactly representable in the sampler.
    constexpr std::size_t frames=44100;
    std::vector<std::int16_t> original(frames);
    constexpr double pi=3.14159265358979323846264338327950288;
    for(std::size_t i=0;i<frames;++i) {
        const double t=double(i)/44100;
        original[i]=std::int16_t(std::lround(
            8000*std::sin(2*pi*311.13*t)+3000*std::sin(2*pi*622.26*t)));
    }
    auto wav=pcm16wav(original,44100,1);
    const auto decoded=akai::wav::decode(wav);
    CHECK(decoded.channels==1 && decoded.frames()==frames && decoded.rate==44100);
    const auto prepared=akai::wav::prepare(decoded,44100);
    CHECK(prepared.frames()==frames && prepared.channels==1);
    const auto dump=akai::sds::encodeStereoAsMono(prepared.stereo,44100,99,0);
    CHECK(dump.frames==frames && dump.packets()==1103);
    CHECK(dump.sysex.size()==21+1103u*127u);
    CHECK(dump.sysex[2]==0 && dump.sysex[4]==99);
    const auto midi=akai::sds::toTimedMidiFile(dump);
    const auto recovered=unpackMidi(midi,dump.packets());
    CHECK(recovered==dump.sysex);
    const auto originalAfterSds=decodeSdsPcm(recovered,frames);
    CHECK(originalAfterSds==original); // lossless 16-bit PCM at native 44.1k
    CHECK(akai::sds::timedMidiTransferMillis(dump)>40000);

    // Adversarial user input: true 48kHz stereo, audible left/right with
    // opposite polarity. The original Akai mono SDS path explicitly downmixes
    // to SILENCE, never picks only channel one or clips at full-scale.
    std::vector<std::int16_t> stereo(480*2);
    for(std::size_t i=0;i<480;++i) {
       stereo[2*i]=16384;stereo[2*i+1]=-16384;
    }
    const auto converted=akai::wav::prepare(
       akai::wav::decode(pcm16wav(stereo,48000,2)),44100);
    CHECK(converted.frames()==441 && converted.channels==2);
    const auto stereoDump=akai::sds::encodeStereoAsMono(
       converted.stereo,44100,99,0);
    CHECK(stereoDump.frames==441 && stereoDump.packets()==12);
    const auto sd=decodeSdsPcm(stereoDump.sysex,stereoDump.frames);
    CHECK(std::all_of(sd.begin(),sd.end(),[](std::int16_t x){return x==0;}));

    // Truncated or malformed source wave must fail BEFORE a MIDI file or a
    // guest ROM import is queued. Confirm no "plausible looking" partial SDS.
    wav.pop_back();
    bool rejected=false;
    try{ (void)akai::wav::decode(wav); }
    catch(const std::exception&){rejected=true;}
    CHECK(rejected);
    std::puts("P037_REAL_WAV_TO_AKAI_SDS_TIMED_MIDI_ROUNDTRIP=PASS");
    return EXIT_SUCCESS;
}
