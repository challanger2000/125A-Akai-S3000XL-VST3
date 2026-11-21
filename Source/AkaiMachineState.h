// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// P017: versioned, bounded transport for original MAME save bytes.
// This never writes directly to Akai sample RAM or reconstructs fake headers.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
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
    return x.size()>=32 && std::memcmp(x.data(),"MAMESAVE",8)==0;
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
} // namespace akai::machine_state
