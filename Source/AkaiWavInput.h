// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// P003 - bounded WAV decoder and offline sample-rate conversion to the
// original Akai S3000XL's existing stereo analogue input stream.
// No guest RAM, sample header, program, firmware or DSP is altered here.
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace akai::wav {
inline constexpr std::size_t maxFileBytes = 64u * 1024u * 1024u;
inline constexpr std::size_t maxConvertedBytes = 32u * 1024u * 1024u;
inline constexpr double pi = 3.14159265358979323846264338327950288;
struct Audio {
    std::uint32_t rate = 0;
    std::uint16_t channels = 0;
    std::vector<float> interleaved;
    std::size_t frames() const noexcept {
        return channels ? interleaved.size() / channels : 0;
    }
};
struct Prepared {
    std::uint32_t rate = 0; // host's output stream rate at preparation
    std::uint16_t channels = 0; // source channels (mono is duplicated only at ADC)
    std::vector<std::int16_t> stereo; // interleaved PCM16, 2 values/frame
    std::size_t frames() const noexcept { return stereo.size() / 2; }
};
inline std::uint16_t le16(std::span<const std::uint8_t> b, std::size_t at) {
    if (at > b.size() || b.size() - at < 2) throw std::runtime_error("Truncated WAV field");
    return std::uint16_t(b[at] | (std::uint16_t(b[at+1]) << 8));
}
inline std::uint32_t le32(std::span<const std::uint8_t> b, std::size_t at) {
    if (at > b.size() || b.size() - at < 4) throw std::runtime_error("Truncated WAV field");
    return std::uint32_t(b[at]) | (std::uint32_t(b[at+1]) << 8)
         | (std::uint32_t(b[at+2]) << 16) | (std::uint32_t(b[at+3]) << 24);
}
inline bool id(std::span<const std::uint8_t> b, std::size_t at, const char* four) {
    return at <= b.size() && b.size() - at >= 4
        && b[at] == std::uint8_t(four[0]) && b[at+1] == std::uint8_t(four[1])
        && b[at+2] == std::uint8_t(four[2]) && b[at+3] == std::uint8_t(four[3]);
}
inline Audio decode(std::span<const std::uint8_t> b) {
    if (b.size() < 44 || b.size() > maxFileBytes || !id(b, 0, "RIFF") || !id(b, 8, "WAVE"))
        throw std::runtime_error("Expected a bounded RIFF/WAVE file");
    const auto riffLength = le32(b, 4);
    if (riffLength < 36 || std::uint64_t(riffLength) + 8 > b.size())
        throw std::runtime_error("Truncated WAV RIFF payload");
    const auto end = std::size_t(riffLength) + 8;
    std::size_t fmtAt = 0, fmtSize = 0, dataAt = 0, dataSize = 0;
    bool hasFmt = false, hasData = false;
    for (std::size_t p = 12; p < end;) {
        if (end - p < 8) throw std::runtime_error("Truncated WAV chunk header");
        const std::size_t length = le32(b, p+4);
        if (length > end - p - 8 || length + (length & 1u) > end - p - 8)
            throw std::runtime_error("WAV chunk is truncated");
        if (id(b,p,"fmt ") && !hasFmt) { hasFmt=true;fmtAt=p+8;fmtSize=length; }
        if (id(b,p,"data") && !hasData) { hasData=true;dataAt=p+8;dataSize=length; }
        p += 8 + length + (length & 1u);
    }
    if (!hasFmt || !hasData || fmtSize < 16)
        throw std::runtime_error("Missing WAV fmt/data chunk");
    auto format = le16(b,fmtAt);
    const auto channels = le16(b,fmtAt+2);
    const auto rate = le32(b,fmtAt+4);
    const auto avgBytes = le32(b,fmtAt+8);
    const auto blockAlign = le16(b,fmtAt+12);
    const auto bits = le16(b,fmtAt+14);
    if (format == 0xfffe) {
        if (fmtSize < 40 || le16(b,fmtAt+16) < 22 || le16(b,fmtAt+18) != bits)
            throw std::runtime_error("Unsupported extensible WAV sample width");
        static constexpr std::uint8_t guidTail[12] =
            {0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71};
        for (int i=0;i<12;++i)
            if (b[fmtAt+28+std::size_t(i)] != guidTail[i])
                throw std::runtime_error("Unsupported WAV subtype GUID");
        const auto subtype = le32(b,fmtAt+24);
        if (subtype != 1 && subtype != 3)
            throw std::runtime_error("Unsupported WAV subtype");
        format = std::uint16_t(subtype);
    }
    const bool pcm = format==1 && (bits==8 || bits==16 || bits==24 || bits==32);
    const bool ieeeFloat = format==3 && bits==32;
    if ((!pcm && !ieeeFloat) || channels<1 || channels>2 || rate<8000 || rate>192000)
        throw std::runtime_error("Unsupported WAV encoding, channel count or sample rate");
    const auto frameBytes = std::uint64_t(channels) * (bits/8);
    if (blockAlign!=frameBytes || avgBytes!=std::uint64_t(rate)*frameBytes ||
        dataSize==0 || dataSize%blockAlign!=0)
        throw std::runtime_error("Invalid WAV frame alignment");
    const auto frames = dataSize / blockAlign;
    if (frames > maxConvertedBytes / (2*sizeof(std::int16_t)))
        throw std::runtime_error("WAV exceeds 32 MiB target capture capacity");
    Audio result;
    result.rate=rate; result.channels=channels;
    result.interleaved.reserve(frames*channels);
    const auto sampleBytes = std::size_t(bits/8);
    for(std::size_t i=0;i<frames;++i) {
        for(unsigned ch=0;ch<channels;++ch) {
            const auto off=dataAt+i*blockAlign+ch*sampleBytes;
            double value=0;
            if(ieeeFloat) {
                value=std::bit_cast<float>(le32(b,off));
                if(!std::isfinite(value) || value < -1.0 || value > 1.0)
                    throw std::runtime_error("Invalid nonfinite/over-full-scale WAV sample");
            } else if(bits==8) value=(double(b[off])-128.0)/128.0;
            else if(bits==16) value=double(std::int16_t(le16(b,off)))/32768.0;
            else if(bits==24) {
                std::int32_t s=std::int32_t(b[off])
                    | (std::int32_t(b[off+1])<<8) | (std::int32_t(b[off+2])<<16);
                if(s & 0x800000) s -= 0x1000000;
                value=double(s)/8388608.0;
            } else value=double(std::int32_t(le32(b,off)))/2147483648.0;
            result.interleaved.push_back(float(value));
        }
    }
    return result;
}
inline float interpolated(const Audio& a, double position, unsigned ch,
                           double targetRate) noexcept {
    const auto count=a.frames();
    if (!count || !std::isfinite(position) || position<0 || position>=double(count))
        return 0;
    ch=std::min(ch,unsigned(a.channels-1));
    // Exact integer sample indices are untouched at the native rate.
    if (a.rate==targetRate)
        return a.interleaved[std::size_t(position)*a.channels+ch];
    // Raised-cosine windowed low-pass sinc. Computed OFF the audio thread.
    // The cutoff prevents obvious aliasing when importing > host sample rate.
    const double cutoff=std::min(1.0,targetRate/double(a.rate))*0.94;
    const auto base=std::int64_t(std::floor(position));
    double sum=0,weight=0;
    for(int k=-15;k<=16;++k) {
        const auto index=base+k;
        if(index<0 || std::uint64_t(index)>=count) continue;
        const double dist=double(index)-position;
        if(std::abs(dist)>=16.0)continue;
        const double x=pi*cutoff*dist;
        const double sinc=std::abs(x)<1.e-10?1.0:std::sin(x)/x;
        const double window=0.5*(1+std::cos(pi*dist/16.0));
        const double w=cutoff*sinc*window;
        sum+=a.interleaved[std::size_t(index)*a.channels+ch]*w;
        weight+=w;
    }
    return weight!=0 ? float(sum/weight) : 0.0f;
}
inline Prepared prepare(const Audio& a, std::uint32_t hostRate) {
    if(a.rate==0 || a.channels<1 || a.channels>2 || a.frames()==0 ||
       hostRate<8000 || hostRate>192000 || !std::isfinite(double(hostRate)))
        throw std::runtime_error("Invalid sample or host rate");
    const double total=std::ceil(double(a.frames())*double(hostRate)/double(a.rate));
    if(total<1 || total>double(maxConvertedBytes/(sizeof(std::int16_t)*2)))
        throw std::runtime_error("Converted WAV exceeds 32 MiB sample limit");
    Prepared result;result.rate=hostRate;result.channels=a.channels;
    const auto outputFrames=std::size_t(total);
    result.stereo.resize(outputFrames*2);
    for(std::size_t i=0;i<outputFrames;++i) {
        const double sourcePosition=double(i)*a.rate/hostRate;
        for(unsigned ch=0;ch<2;++ch) {
            const float raw=interpolated(a,sourcePosition,ch,hostRate);
            const double bounded=std::clamp(double(raw),-1.0,1.0);
            result.stereo[2*i+ch]=std::int16_t(std::clamp(
                std::lround(bounded*32768.0),-32768l,32767l));
        }
    }
    return result;
}
class Feed {
public:
    void assign(const Prepared* p) noexcept { data_=p;position_=0;running_=false;complete_=false; }
    void start() noexcept { if(data_) {position_=0;running_=true;complete_=false;} }
    void stop() noexcept { running_=false; }
    bool running() const noexcept { return running_; }
    bool complete() const noexcept { return complete_; }
    std::size_t position() const noexcept { return position_; }
    bool render(std::int16_t* out, int frames) noexcept {
        if(!running_ || !data_ || !out || frames<=0) return false;
        const auto count=data_->frames();
        for(int i=0;i<frames;++i) {
            const auto at=std::size_t(i)*2;
            if(position_<count) {
                out[at]=data_->stereo[2*position_];
                out[at+1]=data_->stereo[2*position_+1];
                ++position_;
            } else {
                out[at]=0;out[at+1]=0;complete_=true;
            }
        }
        if(position_>=count) complete_=true;
        return true;
    }
private:
    const Prepared* data_=nullptr; // GUI owns in stable double buffer
    std::size_t position_=0;
    bool running_=false,complete_=false;
};
} // namespace akai::wav
