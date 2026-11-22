// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// P017: versioned, bounded transport for original MAME save bytes.
// This never writes directly to Akai sample RAM or reconstructs fake headers.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
namespace akai::machine_state {
inline constexpr std::size_t maxSnapshot = 96u*1024u*1024u;
inline constexpr std::size_t headerBytes = 28;
inline constexpr char signature[8]={'A','K','3','S','N','A','P','1'};
inline std::uint64_t fnv64(std::span<const std::uint8_t> x) noexcept {
    std::uint64_t hash=14695981039346656037ull;
    for(auto c:x){hash^=c;hash*=1099511628211ull;}
    return hash;
}
inline void put32(std::vector<std::uint8_t>& d,std::uint32_t x){
    for(int i=0;i<4;++i)d.push_back(std::uint8_t(x>>(8*i)));
}
inline void put64(std::vector<std::uint8_t>& d,std::uint64_t x){
    for(int i=0;i<8;++i)d.push_back(std::uint8_t(x>>(8*i)));
}
inline std::uint32_t get32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)
        |(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
inline std::uint64_t get64(const std::uint8_t* p) noexcept {
    std::uint64_t v=0;
    for(int i=0;i<8;++i)v|=std::uint64_t(p[i])<<(i*8);
    return v;
}
inline bool hasMameHeader(std::span<const std::uint8_t> x) noexcept {
    // MAME 0.288 save.cpp format 2, Akai S3000XL system name.
    // The live running_machine::save() checks the registry CRC signature
    // separately immediately before any full-machine restore.
    constexpr char machine[]="s3000xl";
    return x.size()>=32 &&
        std::memcmp(x.data(),"MAMESAVE",8)==0 &&
        x[8]==2 && (x[9]&~std::uint8_t(0x02))==0 &&
        std::memcmp(x.data()+10,machine,sizeof(machine))==0;
}
inline bool encode(std::span<const std::uint8_t> image,std::vector<std::uint8_t>& out) {
    if(!hasMameHeader(image)||image.size()>maxSnapshot)return false;
    std::vector<std::uint8_t> tmp;
    tmp.reserve(headerBytes+image.size());
    tmp.insert(tmp.end(),signature,signature+8);
    put32(tmp,1);put32(tmp,std::uint32_t(image.size()));
    put64(tmp,fnv64(image));put32(tmp,0);
    tmp.insert(tmp.end(),image.begin(),image.end());
    out.swap(tmp);return true;
}
inline bool decode(std::span<const std::uint8_t> bytes,std::vector<std::uint8_t>& out) {
    if(bytes.size()<headerBytes+32||bytes.size()>headerBytes+maxSnapshot||
       std::memcmp(bytes.data(),signature,8)!=0||get32(bytes.data()+8)!=1||
       get32(bytes.data()+24)!=0||
       get32(bytes.data()+12)!=bytes.size()-headerBytes)return false;
    const auto image=bytes.subspan(headerBytes);
    if(!hasMameHeader(image)||get64(bytes.data()+16)!=fnv64(image))return false;
    std::vector<std::uint8_t> temp(image.begin(),image.end());
    out.swap(temp);return true;
}

// Native save/load must not replace an outstanding firmware load. The MAME
// public save_or_load_pending() flag remains true if immediate_load deferred
// execution because of anonymous scheduler timers.
inline constexpr bool mayStartSnapshot(bool mamePending, bool nativeRestoreInFlight) noexcept {
    return !mamePending && !nativeRestoreInFlight;
}
// MAME 0.288 immediate_save() may defer until anonymous scheduler timers
// drain. The host may acknowledge a snapshot only when the same request's
// pending MAME operation has resolved, not when immediate_save returned.
class SnapshotBarrier {
public:
    bool begin(std::uint32_t seq, bool mamePending, bool restoreInFlight) noexcept {
        if(active_ || seq==0 || !mayStartSnapshot(mamePending,restoreInFlight))
            return false;
        seq_=seq; active_=true;
        return true;
    }
    bool active() const noexcept { return active_; }
    std::uint32_t sequence() const noexcept { return seq_; }
    bool ready(bool mamePending) const noexcept {
        return active_ && !mamePending;
    }
    void clear() noexcept { active_=false; }
private:
    std::uint32_t seq_=0;
    bool active_=false;
};

// P020: one authoritative, generation-tagged owner-thread restore mailbox.
// A previous restore may finish while a newer DAW project has arrived;
// acknowledging the older work must never cancel the newer restore.
// Source memory remains alive until completion or replacement. All mutex
// operations happen only on MAME/control threads, never the VST audio thread.
class RestoreMailbox {
public:
    using Image = std::shared_ptr<const std::vector<std::uint8_t>>;
    struct Work {
        Image bytes{};
        std::uint64_t generation=0;
        explicit operator bool() const noexcept { return bool(bytes); }
    };
    void publish(Image bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
        image_=std::move(bytes);
        failedGeneration_=0;
        needsService_.store(bool(image_), std::memory_order_release);
    }
    Work take() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!image_ || generation_==appliedGeneration_ ||
            generation_==failedGeneration_) return {};
        return {image_,generation_};
    }
    void finish(const Work& work, bool issued) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!work || work.generation!=generation_ || image_!=work.bytes)
            return; // A newer restore is pending; never discard it.
        if (issued) appliedGeneration_=generation_;
        else failedGeneration_=generation_; // avoid unbounded disk retries
        needsService_.store(false,std::memory_order_release);
    }
    Image unresolved() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return (image_ && generation_!=appliedGeneration_)?image_:Image{};
    }
    Image current() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return image_;
    }
    bool lastAttemptFailed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return image_ && failedGeneration_==generation_;
    }
    bool needsService() const noexcept {
        return needsService_.load(std::memory_order_acquire);
    }
private:
    mutable std::mutex mutex_;
    Image image_{};
    std::uint64_t generation_=0;
    std::uint64_t appliedGeneration_=0;
    std::uint64_t failedGeneration_=0;
    std::atomic<bool> needsService_{false};
};
} // namespace akai::machine_state
