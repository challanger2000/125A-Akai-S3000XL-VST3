#include "../Source/AkaiWavInput.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

static int checks=0;
static void require(bool ok, const char* what) {
    ++checks;
    if(!ok){std::fprintf(stderr,"FAIL %s\n",what);std::exit(EXIT_FAILURE);}
}
static void expectReject(std::vector<std::uint8_t> wav,const char* description) {
    bool rejected=false;
    try{(void)akai::wav::decode(wav);}catch(const std::exception&){rejected=true;}
    require(rejected,description);
}
static void put16(std::vector<std::uint8_t>& b,std::uint16_t x) {
    b.push_back(std::uint8_t(x));b.push_back(std::uint8_t(x>>8));
}
static void put32(std::vector<std::uint8_t>& b,std::uint32_t x) {
    put16(b,std::uint16_t(x));put16(b,std::uint16_t(x>>16));
}
static void four(std::vector<std::uint8_t>& b,const char* s) {
    for(int i=0;i<4;++i)b.push_back(std::uint8_t(s[i]));
}
static void patch32(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t x) {
    for(int i=0;i<4;++i)b[at+i]=std::uint8_t(x>>(8*i));
}
static std::vector<std::uint8_t> wav(std::uint16_t format,std::uint16_t bits,
        std::uint16_t channels,std::uint32_t rate,std::vector<std::uint8_t> pcm,
        bool ext=false) {
    std::vector<std::uint8_t> b;four(b,"RIFF");put32(b,0);four(b,"WAVE");
    four(b,"fmt ");put32(b,ext?40:16);
    put16(b,ext?0xfffe:format);put16(b,channels);put32(b,rate);
    const auto width=std::uint32_t(channels)*(bits/8);
    put32(b,rate*width);put16(b,std::uint16_t(width));put16(b,bits);
    if(ext) {
        put16(b,22);put16(b,bits);put32(b,3);
        put32(b,format);
        static constexpr std::uint8_t tail[] =
            {0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};
        b.insert(b.end(),std::begin(tail),std::end(tail));
    }
    four(b,"data");put32(b,std::uint32_t(pcm.size()));
    b.insert(b.end(),pcm.begin(),pcm.end());
    if(pcm.size()&1)b.push_back(0);
    patch32(b,4,std::uint32_t(b.size()-8));
    return b;
}
static bool near(float a,float b,float eps=0.0001f) {
    return std::abs(a-b)<=eps;
}
int main() {
    using namespace akai::wav;
    const auto pcm8=wav(1,8,1,8000,{0,128,255,64});
    const auto decoded8=decode(pcm8);
    require(decoded8.channels==1&&decoded8.rate==8000&&decoded8.frames()==4,"pcm8_meta");
    require(near(decoded8.interleaved[0],-1),"pcm8_negative_one");
    require(near(decoded8.interleaved[1],0),"pcm8_zero");
    require(near(decoded8.interleaved[2],127.0f/128.0f),"pcm8_positive");
    require(near(decoded8.interleaved[3],-0.5),"pcm8_half_negative");
    auto prepared8=prepare(decoded8,8000);
    require(prepared8.frames()==4 && prepared8.stereo[0]==-32768 &&
            prepared8.stereo[1]==-32768,"mono_to_stereo_duplicated");
    require(prepared8.stereo[2]==0&&prepared8.stereo[3]==0,"mono_center");
    const auto pcm16=wav(1,16,2,44100,{0x00,0x40,0x00,0xc0,0x00,0,0,0});
    const auto decoded16=decode(pcm16);
    require(decoded16.frames()==2 && decoded16.channels==2,"pcm16_stereo");
    require(near(decoded16.interleaved[0],0.5f),"pcm16_left");
    require(near(decoded16.interleaved[1],-0.5f),"pcm16_right");
    auto prepared16=prepare(decoded16,44100);
    require(prepared16.stereo[0]==16384 && prepared16.stereo[1]==-16384,"stereo_independent");
    require(prepared16.stereo[2]==0 && prepared16.stereo[3]==0,"end_of_stereo");
    auto pc24=wav(1,24,1,44100,{0,0,0x80,0xff,0xff,0x7f});
    const auto decoded24=decode(pc24);
    require(near(decoded24.interleaved[0],-1.0f),"pcm24_negative");
    require(decoded24.interleaved[1]>0.999f,"pcm24_positive");
    auto pc32=wav(1,32,1,44100,{0,0,0,0x80,0xff,0xff,0xff,0x7f});
    auto dec32=decode(pc32);
    require(near(dec32.interleaved[0],-1),"pcm32_negative");
    require(dec32.interleaved[1]>0.999f,"pcm32_positive");
    auto f32=wav(3,32,2,48000,{0,0,0,0x3f,0,0,0,0xbf});
    auto fl=decode(f32);
    require(near(fl.interleaved[0],0.5f)&&near(fl.interleaved[1],-0.5f),
           "float_stereo");
    require(decode(wav(3,32,2,48000,{0,0,0,0x3f,0,0,0,0xbf},true)).frames()==1,
           "extensible_float");
    require(decode(wav(1,16,2,44100,{0,0,0,0},true)).channels==2,
           "extensible_pcm");
    auto noData=pcm16;noData[36]='J';
    expectReject(noData,"missing_data_chunk");
    auto corrupt=pcm16;patch32(corrupt,40,0xfffffff0);
    expectReject(corrupt,"truncated_chunk");
    auto invalid=pcm16;invalid[0]='R';invalid[1]='U';
    expectReject(invalid,"wrong_riff");
    auto badRate=pcm16;patch32(badRate,24,200000);
    expectReject(badRate,"unsupported_rate");
    auto fmtBad=pcm16;fmtBad[20]=0x55;fmtBad[21]=0;
    expectReject(fmtBad,"unsupported_format");
    auto shortFile=pcm16;shortFile.pop_back();
    expectReject(shortFile,"incomplete_file");
    auto nan32=wav(3,32,1,44100,{0,0,0xc0,0x7f});
    expectReject(nan32,"nonfinite_pcm");
    auto over32=wav(3,32,1,44100,{0,0,0xa0,0x3f});
    expectReject(over32,"above_full_scale");
    // Offline 96k -> 44.1k resample: stereo stays separate, no clipping.
    std::vector<std::uint8_t> ones;
    for(int i=0;i<960;++i){put16(ones,16384);put16(ones,std::uint16_t(-16384));}
    auto res=prepare(decode(wav(1,16,2,96000,ones)),44100);
    require(res.frames()==441,"resample_length");
    for(std::size_t i=0;i<res.frames();++i) {
        require(res.stereo[2*i]>0&&res.stereo[2*i+1]<0,"stereo_polarity_preserved");
    }
    Feed feed;feed.assign(&prepared16);
    std::int16_t out[8]={17,17,17,17,17,17,17,17};
    require(!feed.render(out,4),"feed_waits_for_firmware_GO");
    require(out[0]==17&&out[1]==17,"no_premature_wav");
    feed.start();
    require(feed.render(out,4),"feed_after_GO");
    require(out[0]==16384&&out[1]==-16384,"first_GO_frame");
    require(out[2]==0&&out[3]==0,"second_GO_frame");
    require(out[4]==0&&out[5]==0&&out[6]==0&&out[7]==0,"tail_silenced");
    require(feed.complete()&&feed.position()==2,"complete_after_data");
    feed.stop();
    require(!feed.render(out,2),"no_feed_after_STOP");
    feed.start();
    require(feed.render(out,2)&&out[0]==16384,"new_GO_restarts");
    Feed empty;empty.assign(nullptr);empty.start();
    require(!empty.render(out,1),"unloaded_source_fail_closed");
    std::printf("AKAI_WAV_INPUT=PASS checks=%d\n",checks);
    return EXIT_SUCCESS;
}
