#pragma once
// This file contains NO ROM bytes. It reads owner-appended ROM material from
// the end of this very VST3 DLL, verified against owner-provided SHA256s.
// Windows PE loader ignores a signed/unsigned overlay appended beyond sections.
// This feature is only for LOCAL/private packing; never publish a ROM-filled DLL.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_cryptography/juce_cryptography.h>
#include <array>
#include <cstring>
#include <cstdint>
#ifdef _WIN32
 #ifndef NOMINMAX
 #define NOMINMAX
 #endif
 #include <windows.h>
#endif

#ifdef _WIN32
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace akai::private_overlay {
inline constexpr char magic[] = "125AAKAIS3KV2ROM"; // exactly 16 bytes, no NUL stored
static_assert(sizeof(magic) == 17);
inline constexpr int firmwareLength = 524288;
inline constexpr int displayLength = 1472;
inline constexpr int trailerLength = 24;
inline std::uint32_t le32(const std::uint8_t* bytes) noexcept {
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8)
         | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}
inline bool readRomsFromOwnDll(juce::MemoryBlock& firmware, juce::MemoryBlock& display) {
#ifdef _WIN32
    // __ImageBase points to THIS VST3 DLL, not the DAW host executable.
    wchar_t path[4096]{};
    const DWORD count = ::GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase), path, DWORD(std::size(path)));
    if (count == 0 || count >= std::size(path))
        return false;
    juce::FileInputStream file(juce::File(juce::String(path)));
    if (file.failedToOpen())
        return false;
    const auto total = file.getTotalLength();
    const auto dataSize = firmwareLength + displayLength;
    if (total < dataSize + trailerLength)
        return false;
    std::array<std::uint8_t, trailerLength> tail{};
    if (!file.setPosition(total - trailerLength)
        || file.read(tail.data(), int(tail.size())) != int(tail.size())
        || std::memcmp(tail.data(), magic, 16) != 0
        || le32(tail.data() + 16) != firmwareLength
        || le32(tail.data() + 20) != displayLength)
        return false;
    juce::MemoryBlock candidateFirmware(firmwareLength);
    juce::MemoryBlock candidateDisplay(displayLength);
    if (!file.setPosition(total - trailerLength - dataSize)
        || file.read(candidateFirmware.getData(), firmwareLength) != firmwareLength
        || file.read(candidateDisplay.getData(), displayLength) != displayLength)
        return false;
    // Never trust a coincidental trailer in another VST3 or a damaged ROM.
    if (juce::SHA256(candidateFirmware).toHexString()
        != "67917482d2a26163d8aee4de676ecbce4fd42859a3f6223c5442426998a086c5"
        || juce::SHA256(candidateDisplay).toHexString()
        != "8738511c6a2f02b0ef1be2cde90898458b4517151cb473ff28ae212dbd1c1737")
        return false;
    firmware = std::move(candidateFirmware);
    display = std::move(candidateDisplay);
    return true;
#else
    (void) firmware;
    (void) display;
    return false;
#endif
}
inline bool stageForOriginalMame(const juce::File& instanceFolder) {
    juce::MemoryBlock firmware, display;
    if (!readRomsFromOwnDll(firmware, display))
        return false;
    const auto roms = instanceFolder.getChildFile("ROMs").getChildFile("s3000xl");
    if (!roms.createDirectory().wasOk())
        return false;
    auto install = [&](const juce::String& name, const juce::MemoryBlock& bytes) {
        const auto file = roms.getChildFile(name);
        if (!file.replaceWithData(bytes.getData(), bytes.getSize()))
            return false;
        juce::MemoryBlock verify;
        return file.loadFileAsData(verify)
            && verify == bytes; // unchanged original bytes through MAME ROM loader
    };
    return install("s3000xl_v2_0.bin", firmware)
        && install("hd61830.bin", display);
}
} // namespace akai::private_overlay
