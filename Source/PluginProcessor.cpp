/*
  ==============================================================================

    S3000XL-VST - S3000XL MAME VST Emulation
    Open Source GPLv2/v3
    https://www.sojusrecords.com

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "AkaiAnalogControls.h"
#include "AkaiFirmwareMonitor.h"
#include "video/hd61830.h"
#include "PluginEditor.h"
#include "BinaryData.h" // MAME layout plugin packaged from official MAME source
#ifdef AKAI_EMBED_PRIVATE_ROMS
#include "AkaiPrivateRomData.h" // generated LOCAL-only, not stored in GitHub
#endif
#ifdef AKAI_RUNTIME_ROM_OVERLAY
#include "AkaiPrivateRomOverlay.h" // no ROM data in public source/binary
#endif

#include <iostream>
#include <chrono>
#ifndef _WIN32
#include <pthread.h>
#endif
#include <stdlib.h>
#include <zlib.h> // MAME uses standard zlib
#include <new>
#include <cstring>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <avrt.h>
#pragma comment(lib, "avrt.lib")
#endif

// ==============================================================================
// MANDATORY MAME MACROS - MUST BE DEFINED BEFORE ANY MAME INCLUDES!
// ==============================================================================
#define PTR64 1
#define LSB_FIRST 1
#define NDEBUG 1
#define __STDC_LIMIT_MACROS 1
#define __STDC_FORMAT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

// --- MAME Core Includes ---
#include "emu.h"
// esqpanel.h removed for S3000XL build (no Ensoniq panel/VFD device)
#include "frontend/mame/ui/ui.h"
#include "osd/modules/lib/osdobj_common.h"
#include "uiinput.h"
#include "inputdev.h"
#include "emuopts.h"
#include "render.h"
#include "osdepend.h"
#include "frontend/mame/mame.h"
#include "frontend/mame/clifront.h"
#include "frontend/mame/mameopts.h"
#include "drivenum.h"
#include "speaker.h"   // [MULTIOUT 2] speaker_device_enumerator, for deriving the mix channel map

// [MULTIOUT 7] THE PLUGIN'S OWN DIAGNOSTICS ARE OFF TOO. Thor: no probe may remain, and the stutter he
// hears is on PLAYBACK of a preset with nothing being recorded - so the whole A/D path is already out
// of the picture and what is left on a per-sample path has to go. The SAMPLER-IN meter walked every
// frame taking two absolute values and two comparisons; that is small, and small per-sample work is
// exactly what this project has been caught by four times.
#define VST_TRACE 0
#define VST_LOG(...) do { if (VST_TRACE && mameMachine != nullptr) mameMachine->logerror(__VA_ARGS__); } while (0)
#include "diimage.h"
#include "imagedev/midiin.h" // P030 actual original Akai UART input-image, not RAM access

#include <iomanip>
#include <fstream>

// MAME Versioning stubs required by the linker
extern const char bare_build_version[] = "0.287";
extern const char bare_vcs_revision[] = "";
extern const char build_version[] = "0.287";

const char * emulator_info::get_appname() { return "mame"; }
const char * emulator_info::get_appname_lower() { return "mame"; }
const char * emulator_info::get_configname() { return "mame"; }
const char * emulator_info::get_copyright() { return "Copyright"; }
const char * emulator_info::get_copyright_info() { return "Copyright"; }

// ==============================================================================
// AUDIO STREAMING & THROTTLING
// ==============================================================================
#include <cmath>      // [SAMPLING] std::abs on ints for the input level meter
#include <algorithm>

// ==============================================================================
// [SAMPLING] LOG CAPTURE - because we were working blind
// ==============================================================================
// Every logerror() in the MAME core went nowhere: this plugin never installed an osd_output, so the
// driver's own instrumentation was invisible and the only evidence available was what the LCD showed.
// That is not a position to debug a sampler from.
//
// FIRST ATTEMPT WAS ON THE WRONG CHANNEL, and the measurement said so straight away: the file was
// created and stayed at ZERO BYTES. osd_output carries osd_printf_* - it does NOT carry logerror.
// running_machine::logerror() walks m_logerror_list, which is what -log itself hooks into
// (machine.cpp:357), so the callback has to be registered with machine.add_logerror_callback().
// A zero-byte file was a better outcome than a plausible-looking one: it named the defect immediately.
//
// Writes %TEMP%\s3000xl-vst.log, flushed per line so a run that ends in a wedge still has its tail on
// disk. Opt-in via %TEMP%\s3000xl-vst-log.enable - a released plugin must not write a log on every
// user's machine.
class VstMameLog
{
public:
    static VstMameLog& instance() { static VstMameLog s; return s; }

    bool start()
    {
        if (started) return true;
        const juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        if (!dir.getChildFile("s3000xl-vst-log.enable").existsAsFile())
            return false;                              // opt-in only
        juce::File f = dir.getChildFile("s3000xl-vst.log");
        f.deleteFile();
        stream = f.createOutputStream();
        if (stream == nullptr) return false;
        started = true;
        return true;
    }

    void write(const char *text)
    {
        if (!started || stream == nullptr || text == nullptr) return;
        const juce::ScopedLock sl(lock);
        stream->writeText(juce::String::fromUTF8(text), false, false, nullptr);
        stream->flush();
    }

private:
    VstMameLog() = default;
    std::unique_ptr<juce::FileOutputStream> stream;
    juce::CriticalSection lock;
    bool started = false;
};

// ==============================================================================
// [SAMPLING] THE INPUT PATH - DAW -> MAME
// ==============================================================================
// Producer: the audio thread, once per processBlock. Consumer: the MAME thread, from
// sound_stream_source_update. Single producer, single consumer, no locks - the same shape as the
// output path above and for the same reason: neither thread may block the other.
//
// It deliberately does NOT throttle. pushAudioFromMame sleeps the MAME thread when the output ring is
// full, because MAME can generate faster than real time. The input side must never do that: blocking
// the audio thread is a dropout, and the DAW is the clock here. If the ring is full the oldest frames
// are dropped and the drop is counted - visible, and bounded, rather than a stall.
void EnsoniqSD1AudioProcessor::pushAudioToMame(const juce::AudioBuffer<float>& buffer,
                                               int numInputChannels, int numSamples)
{
    if (numSamples <= 0)
        return;

    const float* inL = (numInputChannels > 0) ? buffer.getReadPointer(0) : nullptr;
    const float* inR = (numInputChannels > 1) ? buffer.getReadPointer(1) : inL;   // mono -> both sides
    if (inL == nullptr)
        return;

    uint64_t w = samplerInWritten.load(std::memory_order_relaxed);
    const uint64_t r = samplerInRead.load(std::memory_order_acquire);

    // Drop the oldest rather than block. numSamples is a host block, far smaller than the ring.
    // AND COUNT WHAT IS DROPPED. This discarded frames silently for four rounds, which is a hole in
    // the recording and therefore a glitch, and nothing anywhere reported it. The ring has two failure
    // modes and the meter only ever watched one of them.
    if ((w - r) + static_cast<uint64_t>(numSamples) > static_cast<uint64_t>(SAMPLER_IN_FRAMES)) {
        const uint64_t lost = (w - r) + static_cast<uint64_t>(numSamples) - SAMPLER_IN_FRAMES;
        samplerInDropped.fetch_add(lost, std::memory_order_relaxed);
        samplerInRead.store(w + numSamples - SAMPLER_IN_FRAMES, std::memory_order_release);
    }

    // Physical REC GAIN: analogue input attenuation before MAME A/D.
    const float gainTarget = recGainParameter != nullptr
        ? AkaiAnalogControls::normalized(recGainParameter->load(std::memory_order_relaxed)) : 1.0f;
    const float gainStep = AkaiAnalogControls::smoothingCoefficient(
        static_cast<float>(hostSampleRate.load(std::memory_order_relaxed)));
    for (int i = 0; i < numSamples; ++i) {
        smoothedRecGain = AkaiAnalogControls::smooth(smoothedRecGain, gainTarget, gainStep);
        const size_t slot = static_cast<size_t>((w + i) & (SAMPLER_IN_FRAMES - 1)) * SAMPLER_IN_CHANNELS;
        const float left = AkaiAnalogControls::recordInput(inL[i], smoothedRecGain);
        const float right = AkaiAnalogControls::recordInput(inR[i], smoothedRecGain);
        samplerInRing[slot + 0] = static_cast<int16_t>(juce::jlimit(-32768.0f, 32767.0f, left * 32767.0f));
        samplerInRing[slot + 1] = static_cast<int16_t>(juce::jlimit(-32768.0f, 32767.0f, right * 32767.0f));
    }
    samplerInWritten.store(w + numSamples, std::memory_order_release);
}


// P003: transactional WAV staging. The UI/control thread is the only writer
// and all file I/O/decoding/resampling/allocation is confined to this call.
bool EnsoniqSD1AudioProcessor::stageWavForCapture(const juce::File& file,
                                                   juce::String& error)
{
    if (!file.existsAsFile()) {error="The WAV file does not exist.";return false;}
    const auto length=file.getSize();
    if (length<44 || length>static_cast<juce::int64>(akai::wav::maxFileBytes)) {
        error="The WAV must be 44 bytes to 64 MiB in size.";return false;
    }
    // Do not read/decode during playback or before engine readiness changes the
    // accepted ROM state. The staged sample is only fed on a real firmware GO.
    const auto wavReplacementPermitted=[this]() noexcept {
        const auto status=static_cast<akai::wav_capture::State>(
            wavState.load(std::memory_order_acquire));
        const int autoStage=autoCaptureStage.load(std::memory_order_acquire);
        const bool automationActive=akai::capture::isRunningStage(
            static_cast<akai::capture::Stage>(autoStage));
        const bool firmwareRecording=firmwareWadcsKnown.load(std::memory_order_acquire)
            && (firmwareWadcsLatch.load(std::memory_order_acquire)&0x08)!=0;
        return akai::wav_capture::canStage(status,firmwareRecording,automationActive,
            wavPendingSlot.load(std::memory_order_acquire)!=-1,
            wavClearPending.load(std::memory_order_acquire));
    };
    if(!wavReplacementPermitted()) {
        error="WAV capture is in progress or a staged change is pending. "
              "Finish or cancel the Akai recording before importing another WAV.";
        return false;
    }
    try {
        juce::FileInputStream stream(file);
        if (stream.failedToOpen()) {error="Could not open WAV file.";return false;}
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        if (stream.read(bytes.data(),int(bytes.size()))!=int(bytes.size())) {
            error="Incomplete WAV file read.";return false;
        }
        const auto audio=akai::wav::decode(bytes);
        const auto rate=static_cast<std::uint32_t>(std::lround(
            hostSampleRate.load(std::memory_order_acquire)));
        auto prepared=std::make_unique<akai::wav::Prepared>(
            akai::wav::prepare(audio,rate));
        // Never write the slot currently used by MAME. Keep both slots alive
        // until a switch is acknowledged; no shared_ptr atomics on RT threads.
        std::lock_guard<std::mutex> lock(wavStageMutex);
        if(!wavReplacementPermitted()) {
            error="WAV capture began while this WAV was decoding. "
                  "The current Akai recording was preserved.";
            return false;
        }
        // The ORIGINAL Akai RECORD SET-UP time is entered in hundredths
        // of a second, as confirmed with physical panel keys on V2.0 ROM.
        // Add 0.25 s headroom to avoid losing the WAV tail to time rounding.
        const auto centis=(std::uint64_t(prepared->frames())*100u
            +prepared->rate-1u)/prepared->rate;
        if(centis==0 || centis+25u>99999u) {
            error="WAV duration exceeds native Akai 999.99-second REC TIME.";
            return false;
        }
        const int inactive=wavCurrentSlot.load(std::memory_order_acquire)==0?1:0;
        wavSlots[inactive]=std::move(prepared);
        wavTargetRecordCentis.store(std::uint32_t(centis+25u),
                                    std::memory_order_release);
        stagedWavName=file.getFileName();
        wavFramesDelivered.store(0,std::memory_order_release);
        wavState.store(1,std::memory_order_release);
        // Capture the firmware event counter before publishing the asset:
        // GO between GUI publication and engine acceptance must not be lost.
        wavStagedGoSerial.store(wavGoSerial.load(std::memory_order_acquire),
                                std::memory_order_release);
        wavPendingSlot.store(inactive,std::memory_order_release);
        return true;
    } catch(const std::exception& e) {
        error=juce::String(e.what());
        return false;
    }
}
// P030: Prepare standard SDS OFF the DAW audio and MAME owner threads.
bool EnsoniqSD1AudioProcessor::stageWavForOriginalSds(
    const juce::File& file,juce::String& error)
{
    if(!file.existsAsFile() || !file.hasFileExtension(".wav")) {
        error="Choose a valid .wav file.";return false;
    }
    if(!isMameRunningFlag() || !mameIsFullyBooted.load(std::memory_order_acquire)) {
        error="The original Akai firmware must finish booting before SDS import.";
        return false;
    }
    if(originalSdsStage.load(std::memory_order_acquire)!=0) {
        error="This original Akai SDS import session already has a transfer. "
              "Project recall and repeated-import numbering require separate validation.";
        return false;
    }
    const auto capture=static_cast<akai::capture::Stage>(
        autoCaptureStage.load(std::memory_order_acquire));
    if(akai::capture::isRunningStage(capture) ||
       (firmwareWadcsKnown.load(std::memory_order_acquire) &&
        (firmwareWadcsLatch.load(std::memory_order_acquire)&0x08))) {
        error="Do not send MIDI samples during a running Akai ADC recording.";
        return false;
    }
    const auto size=file.getSize();
    if(size<44 || size>static_cast<juce::int64>(akai::wav::maxFileBytes)) {
        error="Original Akai SDS requires a WAV from 44 bytes to 64 MiB.";
        return false;
    }
    try {
        juce::FileInputStream input(file);
        if(input.failedToOpen()) {
            error="Could not open the selected WAV.";return false;
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        if(input.read(bytes.data(),int(bytes.size()))!=int(bytes.size())) {
            error="The original WAV read was incomplete.";return false;
        }
        const auto audio=akai::wav::decode(bytes);
        const auto prepared=akai::wav::prepare(audio,44100); // original Akai mono 44.1k
        const auto number=originalSdsNextSampleNumber.load(std::memory_order_acquire);
        const auto dump=akai::sds::encodeStereoAsMono(
            prepared.stereo,44100,number,0); // owner-ROM tested SysEx logical ID ZERO
        const auto output=juce::File(instanceTempDir).getChildFile("sds")
            .getChildFile("owner-akai-sample-"+juce::String(int(number))+".syx");
        if(output.getParentDirectory().createDirectory().failed() ||
           !output.replaceWithData(dump.sysex.data(),dump.sysex.size())) {
            error="Could not stage the original Akai MIDI Sample Dump.";
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(originalSdsMutex);
            if(originalSdsStage.load(std::memory_order_acquire)!=0) {
                error="An Akai MIDI Sample Dump was queued during WAV preparation.";
                return false;
            }
            originalSdsPendingPath=output.getFullPathName().toStdString();
            originalSdsPendingDurationMs=dump.transportMillis;
            originalSdsLastWavName=file.getFileName();
            originalSdsStage.store(1,std::memory_order_release);
            originalSdsRequested.store(true,std::memory_order_release);
        }
        return true;
    } catch(const std::exception& ex) {
        error=juce::String(ex.what());return false;
    }
}
void EnsoniqSD1AudioProcessor::clearStagedWav() noexcept
{
    // Old immutable assets are not deleted until a future UI-stage overwrite
    // or processor destruction, when MAME is no longer using their memory.
    wavClearPending.store(true,std::memory_order_release);
}

int EnsoniqSD1AudioProcessor::pullAudioForMame(int16_t* dest, int frames)
{
    if (dest == nullptr || frames <= 0)
        return 0;

    uint64_t r = samplerInRead.load(std::memory_order_relaxed);
    const uint64_t w = samplerInWritten.load(std::memory_order_acquire);
    const int have = static_cast<int>(juce::jmin<uint64_t>(static_cast<uint64_t>(frames), w - r));

    for (int i = 0; i < have; ++i) {
        const size_t slot = static_cast<size_t>((r + i) & (SAMPLER_IN_FRAMES - 1)) * SAMPLER_IN_CHANNELS;
        dest[i * SAMPLER_IN_CHANNELS + 0] = samplerInRing[slot + 0];
        dest[i * SAMPLER_IN_CHANNELS + 1] = samplerInRing[slot + 1];
    }
    // Whatever we could not supply is SILENCE, which is what a converter with no signal produces.
    for (int i = have; i < frames; ++i) {
        dest[i * SAMPLER_IN_CHANNELS + 0] = 0;
        dest[i * SAMPLER_IN_CHANNELS + 1] = 0;
    }
    if (have < frames)
        samplerInUnderruns.fetch_add(static_cast<uint64_t>(frames - have), std::memory_order_relaxed);

    // [SAMPLING] A LEVEL METER ON THE WAY IN, so the next run can tell three states apart that all look
    // identical from the machine side: nothing arriving at all (MAME never opened the stream), silence
    // arriving (the stream is live but the host has no input device selected), and real audio arriving.
    // Without this, "the recording is silent" has three possible causes and no way to choose between
    // them.
    // The first version of this counted DELIVERED frames and printed once 44100 of them had
    // accumulated - so when nothing was delivered it printed NOTHING, which is precisely the state it
    // existed to report. An instrument that cannot report the absence it was built to detect is the
    // same mistake this project has now made three times. It counts CALLS instead, and prints
    // unconditionally.
    {
        auto& meter = instanceAudio.meter;
        auto& calls = meter.calls;
        auto& asked = meter.asked;
        auto& got = meter.got;
        // [SAMPLING 121] LEFT AND RIGHT KEPT APART. The old meter took the MAXIMUM of the two, so a
        // silent right channel read exactly like a live one - and this number has been quoted as proof
        // that the input is correct. Thor's hypothesis is that the right side never arrives at all;
        // this is the point in the chain where that is decided.
        auto& peak = meter.peakL;
        auto& peakR = meter.peakR;
        for (int i = 0; VST_TRACE && i < have; i++) {
            const int16_t l = dest[i * SAMPLER_IN_CHANNELS], rr = dest[i * SAMPLER_IN_CHANNELS + 1];
            const int16_t al = int16_t(std::abs(int(l)));
            const int16_t ar = int16_t(std::abs(int(rr)));
            if (al > peak)  peak  = al;
            if (ar > peakR) peakR = ar;
        }
        ++calls; asked += uint64_t(frames); got += uint64_t(have);
        // [SAMPLING 33] every 2000 calls, not 200: the last run wrote 29596 of these lines, and a
        // meter that costs real time on a path whose whole problem is real time is self-defeating.
        if ((calls % 2000) == 0) {
            // OCCUPANCY AND DROPS are the two numbers that were missing, and between them they say
            // which clock is ahead. Occupancy pinned near full with drops climbing means MAME is
            // consuming slower than the DAW produces - the machine is behind, and the recording gets
            // holes in it. Occupancy near zero with underruns means the opposite. A steady mid-level
            // with neither means the two clocks are in step and the fault is somewhere else entirely.
            const uint64_t w = getSamplerInWritten(), r = getSamplerInRead();
            const uint64_t fill = w - r;
            const uint64_t dropped = samplerInDropped.load(std::memory_order_relaxed);
            auto& lastDropped = meter.lastDropped;
            const uint64_t dropNow = dropped - lastDropped;
            lastDropped = dropped;

            // THE NUMBER THAT DECIDES IT: frames per SECOND, on each side. Everything so far has been
            // counted per call, and a per-call count cannot tell a rate error from jitter - which is
            // the whole question. If MAME consumes materially fewer frames per second than the DAW
            // produces, the ring must overflow no matter how large it is, and no amount of buffering
            // will fix it; if the two rates match and the fill still swings, it is jitter and a deeper
            // ring or a smoother drop policy is the answer. Those need opposite fixes.
            auto& lastT = meter.lastT;
            auto& lastW = meter.lastW;
            const double nowT = (mameMachine != nullptr) ? mameMachine->time().as_double() : 0.0;
            const double dt = nowT - lastT;
            const double inRate      = (dt > 0.0) ? double(w - lastW) / dt : 0.0;   // DAW -> ring
            const double consumeRate = (dt > 0.0) ? double(got)       / dt : 0.0;   // ring -> MAME
            lastT = nowT; lastW = w;

            if (mameMachine != nullptr)
                // The previous build printed "PEAK=   <<<<" with the tail literal, because an earlier
                // edit added the rate arguments and DROPPED the two peak arguments while leaving their
                // conversions in the format string. A meter whose own output is malformed is worth
                // nothing, and this one lost its level reading for a whole round.
                // HOST RATE, the one number still missing. IN is counted in frames pushed by
                // processBlock but divided by EMULATED seconds, so the same figure means completely
                // different things at 44100 and at 96000: at 44100 an IN of 56448 says the emulation
                // is running at 78 percent of real time, while at 96000 it would say the opposite.
                // Every reading of that ratio so far has rested on an assumption about this.
                VST_LOG("SAMPLER-IN calls=%llu got=%llu fill=%llu/%d DROPPED=%llu (total %llu) TRIM=%llu HOST=%.0f IN=%.0f/s OUT=%.0f/s ratio=%.3f PEAK-L=%.1f%% PEAK-R=%.1f%%%s\n",
                    (unsigned long long)calls, (unsigned long long)got,
                    (unsigned long long)fill, SAMPLER_IN_FRAMES,
                    (unsigned long long)dropNow, (unsigned long long)dropped,
                    (unsigned long long)samplerInCorrections.load(std::memory_order_relaxed),
                    getHostSampleRate(),
                    inRate, consumeRate, (consumeRate > 0.0) ? inRate / consumeRate : 0.0,
                    double(peak) * 100.0 / 32767.0, double(peakR) * 100.0 / 32767.0,
                    (dropNow > 0) ? "   <<<< DROPPING - MAME is consuming slower than the DAW produces"
                  : (got == 0)   ? "   <<<< RING EMPTY - processBlock is not pushing"
                  : (peak == 0 && peakR == 0) ? "   <<<< DELIVERED BUT SILENT"
                  : (peakR == 0) ? "   <<<< RIGHT CHANNEL IS SILENT AT THE PLUGIN"
                  : (peak == 0)  ? "   <<<< LEFT CHANNEL IS SILENT AT THE PLUGIN"
                                 : "   <<<< clean");
            asked = 0; got = 0; peak = 0; peakR = 0;
        }
    }

    // [SAMPLING] DRIFT CORRECTION - the last known fault, and the right shape of answer for it.
    //
    // The DAW's audio clock and MAME's emulated clock are independent and free-running. Measured this
    // run with the host rate finally printed: HOST=44100, OUT pinned at 44100/s, ratio oscillating
    // 0.988 to 1.145 around unity. So there is NO systematic rate error - what remains is ordinary
    // drift, and drift accumulates without bound. A deeper ring cannot fix that; it only sets how long
    // it takes before the ring hits a wall and hundreds of frames are discarded at once, which is what
    // was audible as glitching.
    //
    // So hold the fill near a target instead of waiting for a wall: at most ONE frame of correction
    // every 64 calls, skipping a frame when the ring runs long and repeating one when it runs short.
    // At roughly 44100 calls a second that is about 690 frames per second of authority against a drift
    // measured in single frames per second - orders of magnitude more than needed, applied one frame at
    // a time so it is spread out and inaudible. The failure it replaces was the same total correction
    // delivered in bursts of hundreds, which is precisely why it could be heard.
    const uint64_t fillAfter = w - (r + static_cast<uint64_t>(have));
    const int adjust = instanceAudio.drift.next(fillAfter, have);
    if (adjust != 0)
        samplerInCorrections.fetch_add(1, std::memory_order_relaxed);

    samplerInRead.store(r + have + adjust, std::memory_order_release);

    // The MAME input callback owns this live WAV feeder. No host callback,
    // UI thread or timer drives its playback and there is no filesystem work.
    if (wavClearPending.exchange(false,std::memory_order_acq_rel)) {
        wavFeed.assign(nullptr);
        wavCurrentSlot.store(-1,std::memory_order_release);
        wavState.store(0,std::memory_order_release);
        wavPendingSlot.store(-1,std::memory_order_release);
        wavSeenGo=wavGoSerial.load(std::memory_order_acquire);
        wavSeenStop=wavStopSerial.load(std::memory_order_acquire);
    } else {
        const int requested=wavPendingSlot.load(std::memory_order_acquire);
        if (requested>=0 && requested<=1) {
            wavFeed.assign(wavSlots[requested].get());
            wavSeenGo=wavStagedGoSerial.load(std::memory_order_acquire);
            wavSeenStop=wavStopSerial.load(std::memory_order_acquire);
            wavSmoothedGain=recGainParameter
                ? AkaiAnalogControls::normalized(recGainParameter->load(std::memory_order_relaxed))
                : 1.0f;
            wavCurrentSlot.store(requested,std::memory_order_release);
            wavState.store(1,std::memory_order_release); // accepted/armed
            wavPendingSlot.store(-1,std::memory_order_release); // acknowledge
        }
    }
    const auto stop=wavStopSerial.load(std::memory_order_acquire);
    if(stop!=wavSeenStop) {
        wavSeenStop=stop;
        // Preserve the WAV completion signal through the original STOP
        // edge, which can occur between two LCD/controller updates.
        const auto next=akai::wav_capture::afterFirmwareStop(
            static_cast<akai::wav_capture::State>(
                wavState.load(std::memory_order_acquire)),
            wavFeed.complete(),
            wavCurrentSlot.load(std::memory_order_acquire)>=0);
        wavFeed.stop();
        wavState.store(static_cast<int>(next),std::memory_order_release);
    }
    const auto go=wavGoSerial.load(std::memory_order_acquire);
    if(go!=wavSeenGo) {
        wavSeenGo=go;
        // If GO and STOP happened between two input pulls, fail closed
        // rather than starting a sample after the firmware has stopped.
        if((firmwareWadcsLatch.load(std::memory_order_acquire) & 0x08)!=0
            && wavCurrentSlot.load(std::memory_order_acquire)>=0) {
            wavFeed.start(); // FIRST original firmware GO, never on file load
            wavFramesDelivered.store(0,std::memory_order_release);
            wavState.store(2,std::memory_order_release);
        }
    }
    if(wavFeed.render(dest,frames)) {
        // This is the original A/D input, not sample RAM. Apply REC GAIN
        // to imported audio as well; independent gain smoothing belongs to
        // the MAME thread so it cannot race the host's audio callback.
        const float captureGain=recGainParameter
            ? AkaiAnalogControls::normalized(recGainParameter->load(std::memory_order_relaxed))
            : 1.0f;
        const float step=AkaiAnalogControls::smoothingCoefficient(
            static_cast<float>(hostSampleRate.load(std::memory_order_relaxed)));
        for(int i=0;i<frames;++i) {
            wavSmoothedGain=AkaiAnalogControls::smooth(wavSmoothedGain,captureGain,step);
            for(int ch=0;ch<SAMPLER_IN_CHANNELS;++ch) {
                const auto at=i*SAMPLER_IN_CHANNELS+ch;
                dest[at]=static_cast<int16_t>(std::clamp(
                    std::lround(double(dest[at])*wavSmoothedGain),-32768l,32767l));
            }
        }
        wavFramesDelivered.store(static_cast<uint64_t>(wavFeed.position()),std::memory_order_release);
        if(wavFeed.complete())wavState.store(3,std::memory_order_release);
        return frames;
    }
    return have;
}

void EnsoniqSD1AudioProcessor::pushAudioFromMame(const int16_t* pcmBuffer, int numSamples) {
    if (!isMameRunningFlag()) return;

    // AUDIO-DRIVEN THROTTLING:
    // MAME runs on a separate thread and can generate audio much faster than real-time.
    // We throttle the MAME thread here by putting it to sleep if our ring buffer has 
    // more unread samples than the defined threshold. This prevents buffer overflows.
    
        while (isMameRunningFlag()) {
            
                        // --- DEADLOCK BREAKER ---

                        if (requestMameSave.load(std::memory_order_acquire)
                            || requestMameLoad.load(std::memory_order_acquire)
                            || nativeRestore.needsService()
                            || nativeSnapshotRequested.load(std::memory_order_acquire)
                               != nativeSnapshotFinished.load(std::memory_order_acquire)) {
                            // P017: a throttled native MAME sink must not block
                            // the only owner thread that can execute state I/O.
                            break;
                        }
                        
                        // --- ANCHOR DEADLOCK BREAKER ---

                        if (needAnchorSync.load(std::memory_order_acquire)) {
                            break;
                        }
            
            uint64_t writePos = getTotalWritten();
            uint64_t readPos = getTotalRead();
            int64_t available = writePos - readPos;

            int maxAllowedBuffer = mameBufferThreshold.load(std::memory_order_relaxed);
            
            if (isNonRealtime()) {
                maxAllowedBuffer = maxOfflineBuffer.load(std::memory_order_relaxed);
            }
                               
            if (available < maxAllowedBuffer) {
                break; // There is enough room, write
            }

            // Buffer is full. Sleep the MAME thread until processBlock consumes some data.
            
// --- OPTIMIZATION START ---
#ifdef _WIN32
        // Default Windows scheduler resolution is ~15.6ms. Waiting for 5ms might 
        // put the thread to sleep for 16ms, causing severe buffer underruns in MAME.
        // We force a much tighter wake-up interval on Windows.
            mameThrottleEvent.wait(1);
#else
        // Original macOS behavior preserved
            mameThrottleEvent.wait(5);
#endif
            // --- OPTIMIZATION END ---
        }
        
    uint64_t currentWritePos = totalWritten.load(std::memory_order_relaxed);
        
    if (needAnchorSync.load(std::memory_order_acquire) && mameMachine != nullptr) {
            anchorMameTime.store(mameMachine->time().as_double(), std::memory_order_relaxed);
            anchorDawSample.store(currentWritePos, std::memory_order_relaxed);
            needAnchorSync.store(false, std::memory_order_release);
        }
        
    // [MULTIOUT 2] THE CHANNEL MAP IS DERIVED FROM THE RUNNING MACHINE, NOT HARDCODED.
    //
    // This used to read: "MAME record mix = 3 interleaved outputs, ch0 = FLOPPY, ch1 = MAIN L,
    // ch2 = MAIN R, STRIDE = 3". That was a real measurement, taken with -wavwrite against the speaker
    // set of the time - and adding one speaker to the driver makes every number in it wrong. A
    // measurement that silently expires is worse than none, because nothing announces the expiry.
    //
    // MAME's own layout rule, from emu/sound.cpp and not from inference:
    //
    //     m_outputs_count = 0;
    //     for (speaker_device &dev : speaker_device_enumerator(root)) {
    //         m_speakers.emplace_back(dev, rate, m_outputs_count);   // first_output = running total
    //         m_outputs_count += dev.inputs();
    //     }
    //
    // So the channels lie in speaker-enumeration order, each speaker taking inputs() of them, and the
    // record buffer is interleaved with a stride of outputs_count(). Both are readable at runtime, so
    // the map is computed ONCE by walking the same enumerator and matching the speaker TAG - which
    // survives a driver change instead of quietly describing the previous one.
    auto& mixStride = instanceAudio.output.stride;
    auto& mainOffset = instanceAudio.output.main;
    auto& indivOffset = instanceAudio.output.indiv;
    if (mixStride == 0 && mameMachine != nullptr)
    {
        instanceAudio.output.begin(int(mameMachine->sound().outputs_count()));
        int first = 0;
        juce::String layout;
        for (speaker_device &spk : speaker_device_enumerator(mameMachine->root_device()))
        {
            const std::string tag = spk.tag();
            const int n = int(spk.inputs());
            instanceAudio.output.observe(tag, first, n);
            layout += juce::String(tag) + "@" + juce::String(first) + "x" + juce::String(n) + "  ";
            first += n;
        }
        VST_LOG("MIXMAP stride=%d  main=%d  indiv=%d  |  %s\n",
                              mixStride, mainOffset, indivOffset, layout.toRawUTF8());
    }

    if (instanceAudio.output.hasMain())
    {
    for (int i = 0; i < numSamples; ++i) {

                int index = currentWritePos & (RING_BUFFER_SIZE - 1);

                ringBufferL[index]    = pcmBuffer[i * mixStride + mainOffset + 0] / 32768.0f;
                ringBufferR[index]    = pcmBuffer[i * mixStride + mainOffset + 1] / 32768.0f;

                // The floppy speaker is still deliberately dropped - no disk rattle in the instrument
                // output - and it is dropped by NOT being looked up, rather than by an assumption about
                // which channel it occupies. Measured: enable_sound(false) does NOT remove it, because
                // floppy_image_device::device_add_mconfig creates the speaker unconditionally. Deriving
                // the map is what caught that; a hardcoded stride would have shifted every channel.
                if (instanceAudio.output.hasIndividuals())
                    for (int c = 0; c < INDIV_OUTS; ++c)
                        ringBufferIndiv[c][index] = pcmBuffer[i * mixStride + indivOffset + c] / 32768.0f;

        currentWritePos++;
    }
    }
    
    totalWritten.store(currentWritePos, std::memory_order_release);
    
    if (needAnchorSync.load(std::memory_order_acquire) && mameMachine != nullptr) {
        anchorMameTime.store(mameMachine->time().as_double(), std::memory_order_relaxed);
        anchorDawSample.store(currentWritePos, std::memory_order_relaxed);
        needAnchorSync.store(false, std::memory_order_release);
    }

}
    
// ==============================================================================
// VST MIDI PORT IMPLEMENTATION (TIMESTAMPED)
// ==============================================================================
class VstMidiInputPort : public osd::midi_input_port {
private:
    EnsoniqSD1AudioProcessor* processor;
public:
    VstMidiInputPort(EnsoniqSD1AudioProcessor* p) : processor(p) {}
    virtual ~VstMidiInputPort() {}
    
    virtual bool poll() override {
        return processor->pollMidiData();
    }
    
    virtual int read(uint8_t *pOut) override {
        if (processor->pollMidiData()) {
            *pOut = static_cast<uint8_t>(processor->readMidiByte());
            return 1;
        }
        return 0;
    }
};

// ==============================================================================
// VST MIDI OUTPUT PORT (SD-1 DUART TX → JUCE MIDI Out)
// ==============================================================================
class VstMidiOutputPort : public osd::midi_output_port {
private:
    EnsoniqSD1AudioProcessor* processor;
public:
    VstMidiOutputPort(EnsoniqSD1AudioProcessor* p) : processor(p) {}
    virtual ~VstMidiOutputPort() {}
    
    virtual void write(uint8_t data) override {
        processor->pushMidiOutByte(data);
    }
};

// ==============================================================================
// MIDI OUTPUT RING BUFFER
// ==============================================================================
void EnsoniqSD1AudioProcessor::pushMidiOutByte(uint8_t data) {
    int currentWrite = midiOutWritePos.load(std::memory_order_relaxed);
    int nextWrite = (currentWrite + 1) & (MIDI_OUT_BUFFER_SIZE - 1);
    if (nextWrite != midiOutReadPos.load(std::memory_order_acquire)) {
        midiOutBuffer[currentWrite] = data;
        midiOutWritePos.store(nextWrite, std::memory_order_release);
    }
}

// ==============================================================================
// TIMESTAMPED MIDI QUEUE LOGIC (PURE ANCHOR)
// ==============================================================================
void EnsoniqSD1AudioProcessor::pushMidiByte(uint8_t data, double targetMameTime) {
    int currentWrite = midiWritePos.load(std::memory_order_relaxed);

    // --- OPTIMIZATION ---
    // Fast wrap-around for MIDI ring buffer
    int nextWrite = (currentWrite + 1) & (MIDI_BUFFER_SIZE - 1);

    if (nextWrite != midiReadPos.load(std::memory_order_acquire)) {
        midiBuffer[currentWrite].data = data;
        midiBuffer[currentWrite].targetMameTime = targetMameTime;
        midiWritePos.store(nextWrite, std::memory_order_release);
    }
}

// ========================================================
// FORCE COMPARE OFF
// ========================================================
void EnsoniqSD1AudioProcessor::forceCompareOff()
{
    if (!mameMachine) return;
    
    // Check if we are in the File Manager (using the saved state struct)
    if (fileManagerState.visible || requestedViewIndex.load() == 4)
        {
            juce::String cat = fileManagerState.category;
            
            //CHECK IF INTERNAL (INT, ROM, CART)
            bool isInternal = (cat == "INT (RAM)") ||
                                      (cat == "ROM0") ||
                                      (cat == "ROM1") ||
                                      (cat == "CART");

            // IF EXTERNAL THEN EXIT
            if (!isInternal && cat.isNotEmpty())
            {
                return;
            }
        }
    
    auto* osram_share = mameMachine->root_device().memshare("osram");
    if (!osram_share || osram_share->bytes() <= 0x92DC) return;
    
    uint8_t* osram = static_cast<uint8_t*>(osram_share->ptr());
    if (osram[0x92DC] != 0x00)
        {
            double now = mameMachine->time().as_double();
            
            // No incoming DAW MIDI
            suppressMidiInput.store(true, std::memory_order_release);
            pushMidiByte(0xF7, now + 0.01);
            
            // Helper lambda to send a properly nibblized Virtual Button press
            auto sendBtn = [&](int buttonNum, double downTime, double upTime) {
                        
                        // Button DOWN: F0 0F 05 00 00 00 00 00 [hi lo] F7
                        uint8_t downMsg[11] = {
                            0xF0, 0x0F, 0x05, 0x00, 0x00, 0x00,  // header (ch=0, msgType=0x00)
                            0x00, 0x00,                          // command type 0x00 nibblized
                            static_cast<uint8_t>((buttonNum >> 4) & 0x0F),
                            static_cast<uint8_t>(buttonNum & 0x0F),
                            0xF7
                        };
                        for (int i = 0; i < 11; ++i) {
                            pushMidiByte(downMsg[i], downTime + i * 0.00035);
                        }
                        
                        // Button UP: buttonNum + 96
                        int upNum = buttonNum + 96;
                        uint8_t upMsg[11] = {
                            0xF0, 0x0F, 0x05, 0x00, 0x00, 0x00,
                            0x00, 0x00,
                            static_cast<uint8_t>((upNum >> 4) & 0x0F),
                            static_cast<uint8_t>(upNum & 0x0F),
                            0xF7
                        };
                        for (int i = 0; i < 11; ++i) {
                            pushMidiByte(upMsg[i], upTime + i * 0.00035);
                        }
                    };
                    
            // Compare (63) — toggles compare OFF
            sendBtn(63, now + 0.5, now + 0.8);

            // Async turn on MIDI input after — unless a file manager close cancelled it first
            juce::Timer::callAfterDelay(1000, [this]() {
                if (!midiOpCancelled.load(std::memory_order_acquire))
                    suppressMidiInput.store(false, std::memory_order_release);
            });
        }
}

void EnsoniqSD1AudioProcessor::clearMidiBuffer() {
    // Clear MIDI buffer
    midiReadPos.store(midiWritePos.load(std::memory_order_acquire), std::memory_order_release);
}

bool EnsoniqSD1AudioProcessor::pollMidiData() {
    if (mameMachine == nullptr) return false;
    
    int currentRead = midiReadPos.load(std::memory_order_relaxed);
    if (currentRead == midiWritePos.load(std::memory_order_acquire)) return false;
    
    // The MAME processor accurately waits for the calculated microsecond!
    return mameMachine->time().as_double() >= midiBuffer[currentRead].targetMameTime;
}

int EnsoniqSD1AudioProcessor::readMidiByte() {
    if (mameMachine == nullptr) return 0;

    int currentRead = midiReadPos.load(std::memory_order_relaxed);
    int currentWrite = midiWritePos.load(std::memory_order_acquire);
    if (currentRead == currentWrite) return 0;

    if (mameMachine->time().as_double() >= midiBuffer[currentRead].targetMameTime) {
        uint8_t data = midiBuffer[currentRead].data;

        // Fast wrap-around using bitwise AND
        int nextRead = (currentRead + 1) & (MIDI_BUFFER_SIZE - 1);
        midiReadPos.store(nextRead, std::memory_order_release);

        return data;
    }
    return 0;
}

// ==============================================================================
// HEADLESS OSD (Operating System Dependent) INTERFACE
// This class acts as the bridge between MAME's core and the JUCE environment.
// ==============================================================================
class VstOsdInterface : public osd_common_t
{
private:
    EnsoniqSD1AudioProcessor* processor;
    running_machine* mame_machine = nullptr;
    uint64_t lastFrameHash = 0;
    uint32_t lastMouseButtons = 0;
    int audioMacroHeldBank = -1;   // audio-thread-only: which bank is electronically held
    akai::capture::Controller captureController; // MAME/OSD owner thread only
    int captureHeldIndex = -1; // only release buttons this controller pressed
    bool originalSdsTransferActive=false; // native MAME owner only
    std::uint64_t originalSdsFinishAtMs=0;
    akai::machine_state::RestoreMailbox::Work nativeRestoreInFlight{}; // MAME owner only
    akai::machine_state::SnapshotBarrier nativeSnapshotInFlight{}; // MAME owner only
    
    render_target* main_target = nullptr;

    int saveFrameDelay = 0;
    int loadFrameDelay = 0;
    int frameSkipCounter = 0;
    int bootGraceFrames = 0;
    
public:
    
    virtual void process_events() override {}
    virtual bool has_focus() const override { return true; }
    
    VstOsdInterface(EnsoniqSD1AudioProcessor* p, osd_options &options)
    : osd_common_t(options), processor(p) {}
    
    virtual ~VstOsdInterface() {}
    
    virtual void init(running_machine &machine) override {
        
        // REQUIRED: Initializes MAME's core sound, mouse, and keyboard modules
        osd_common_t::init(machine);
              
        mame_machine = &machine;
        processor->mameMachine = &machine;

        // [SAMPLING] Capture the driver's logerror output. This is where it has to be hooked - see
        // VstMameLog for why osd_output was the wrong channel and how a zero-byte file proved it.
        if (VstMameLog::instance().start())
            machine.add_logerror_callback([](const char *line) { VstMameLog::instance().write(line); });
        
        // Observe the original firmware WADCS port-A control value without
        // inventing a separate GUI toggle or adding per-sample logging.
        machine.output().set_notifier("s3000xl_wadcs",
            EnsoniqSD1AudioProcessor::mameOutputNotifier, processor);
                        
        int targetViewIdx = processor->requestedViewIndex.load(std::memory_order_acquire);
        int startW = processor->windowWidth.load(std::memory_order_acquire);
        int startH = processor->windowHeight.load(std::memory_order_acquire);

        if (startW <= 0) startW = 1200;
        if (startH <= 0) startH = 539;

        // SINGLE ALLOCATION with exact dimensions
        main_target = machine.render().target_alloc();
        main_target->set_bounds(startW, startH);

        int mameViewMap[4] = { 0, 0, 0, 0 }; // S3000XL: layout has ONE view (0); all plugin views map to it
        main_target->set_view(mameViewMap[targetViewIdx % 4]);
        
    };
        
    virtual void osd_exit() override {
        if (mame_machine != nullptr && main_target != nullptr) {
            // Gracefully return the render target to MAME to avoid crashes during cleanup
            mame_machine->render().target_free(main_target);
            main_target = nullptr;
        }
        if (processor != nullptr) {
                processor->firmwareWadcsKnown.store(false, std::memory_order_release);
                processor->autoCaptureStage.store(
                    int(akai::capture::Stage::Cancelled),std::memory_order_release);
                processor->mameMachine = nullptr;
            }
        osd_common_t::osd_exit();
    }
    
    virtual void update(bool skip_redraw) override {
                
        if (mame_machine == nullptr) return;
        
            // Set the flag on the very first frame update.
            // This confirms that MAME device clocks are initialized and safe for time calculations.
            if (!processor->mameIsFullyBooted.load(std::memory_order_relaxed)) {
                processor->mameIsFullyBooted.store(true, std::memory_order_release);
            }
        
        if (!processor->isMameRunningFlag()) {
                mame_machine->schedule_exit();
                return;
            }
                
                // ==============================================================================
                // --- THE PERFECT SILENT WATCHER (DIRECT MEMSHARE, CRASH-PROOF) ---
                // ==============================================================================
                // Bypassing the CPU API (which causes Access Violation on the video thread).
                // Instead, we write directly to the raw C++ memory array of "osram"!
                if (mame_machine != nullptr) {
                    auto* osram_share = mame_machine->root_device().memshare("osram");
                    
                    // Only proceed if RAM exists and has been allocated
                    if (osram_share != nullptr && osram_share->bytes() >= 0xCE0C) {
                        uint8_t* osram = static_cast<uint8_t*>(osram_share->ptr());
                        
                        // The CPU writes to 0xFFCE0B. In the Little-Endian array, this is 0xCE0A or 0xCE0B.
                        // To be safe and prevent freezes, we keep both locked at 0x01!
                        // No thread collision, no CPU calls, just raw byte overwriting.
                        if (osram[0xCE0A] != 0x01) osram[0xCE0A] = 0x01;
                        if (osram[0xCE0B] != 0x01) osram[0xCE0B] = 0x01;
                    }
                }
        
        // NOTE: do NOT early-return on skip_redraw. Under -nothrottle MAME frequently frameskips
        // (skip_redraw=true), which used to gate the LCD frame-grab below -> the panel only
        // refreshed when an event forced a rendered frame. We grab the frame every update() so the
        // LCD reflects the live machine (e.g. sample-load progress). The frameSkipCounter (%2) and
        // the dirty-frame hash keep the cost bounded.
        juce::ignoreUnused(skip_redraw);
                
        // --- DYNAMIC VIEW & RESIZE SWITCHING ---
        bool viewChanged = processor->requestViewChange.exchange(false, std::memory_order_acquire);
        bool sizeChanged = processor->requestRenderResize.exchange(false, std::memory_order_acquire);

                if (viewChanged || sizeChanged) {
                    int targetViewIdx = processor->requestedViewIndex.load(std::memory_order_acquire);
                    int newW = processor->windowWidth.load(std::memory_order_acquire);
                    int newH = processor->windowHeight.load(std::memory_order_acquire);

                    /*if (newW > 0 && newH > 0) {
                        if (viewChanged) {
                            int mameViewMap[4] = { 0, 0, 0, 0 }; // S3000XL: layout has ONE view (0); all plugin views map to it
                            main_target->set_view(mameViewMap[targetViewIdx % 4]);
                        }
                        main_target->set_bounds(newW, newH);
                    }*/
                    // debug rack
#ifdef SD1_DEBUG_RACK_PANEL
                    if (newW > 0 && newH > 0) {
                                            if (viewChanged) {
                                                int mameViewMap[4] = { 0, 0, 0, 0 }; // S3000XL: layout has ONE view (0); all plugin views map to it
                                                int effectiveView = (targetViewIdx == 4) ? 2 : (targetViewIdx % 4);
                                                main_target->set_view(mameViewMap[effectiveView]);
                                            }
                                            main_target->set_bounds(newW, newH);
                                        }
#else
                    if (newW > 0 && newH > 0) {
                        if (viewChanged) {
                            int mameViewMap[4] = { 0, 0, 0, 0 }; // S3000XL: layout has ONE view (0); all plugin views map to it
                            main_target->set_view(mameViewMap[targetViewIdx % 4]);
                        }
                        main_target->set_bounds(newW, newH);
                    }
#endif // SD1_DEBUG_RACK_PANEL
                }
                        
        // P017 native full MAME snapshot. Never use unsafe guest-RAM reads
        // to simulate a complete project state. All MAME save/restore operations
        // run ONLY on this MAME owner thread, outside the host audio callback.
        // MAME 0.288 immediate_load is NOT guaranteed immediate:
        // pending anonymous timers can defer it to a later machine timeslice.
        // Never let a subsequent save or new load overwrite that pending LOAD.
        if (nativeRestoreInFlight && !mame_machine->save_or_load_pending()) {
            processor->nativeRestore.finish(nativeRestoreInFlight,true);
            nativeRestoreInFlight={};
        }
        if (!nativeRestoreInFlight && !nativeSnapshotInFlight.active()
            && !mame_machine->save_or_load_pending()) {
            if (const auto work=processor->nativeRestore.take()) {
                bool issued=false;
                if(work.bytes->size()>=32
                   && work.bytes->size()<=akai::machine_state::maxSnapshot) {
                    auto dest=processor->nativeSnapshotFile();
                    if(dest.getParentDirectory().createDirectory().wasOk()) {
                        dest.deleteFile(); // no stale snapshot on failed write
                        auto stream=dest.createOutputStream();
                        if(stream && stream->openedOk()
                            && stream->write(work.bytes->data(),work.bytes->size())) {
                            stream->flush();
                            stream.reset();
                            mame_machine->immediate_load(
                                dest.getFullPathName().toStdString());
                            issued=true;
                        }
                    }
                }
                if (!issued) {
                    processor->nativeRestore.finish(work,false);
                } else if (mame_machine->save_or_load_pending()) {
                    // Keep the owner image alive until the scheduler resolves
                    // the load (or aborts it). Completion is not PCM proof.
                    nativeRestoreInFlight=work;
                } else {
                    processor->nativeRestore.finish(work,true);
                }
            }
        }
        // Complete only the original generation after the MAME scheduler
        // has resolved its own possibly deferred full-machine .sta SAVE.
        auto finishSnapshot = [&]() {
            const auto file=processor->nativeSnapshotFile();
            bool valid=file.existsAsFile() && file.getSize()>=32
                && file.getSize()<=static_cast<juce::int64>(akai::machine_state::maxSnapshot);
            if(valid) {
                juce::FileInputStream read(file);
                char magic[8]{};
                valid=read.openedOk() && read.read(magic,8)==8
                    && std::memcmp(magic,"MAMESAVE",8)==0;
            }
            processor->nativeSnapshotSucceeded.store(valid,std::memory_order_release);
            processor->nativeSnapshotFinished.store(nativeSnapshotInFlight.sequence(),
                                                    std::memory_order_release);
            nativeSnapshotInFlight.clear();
            processor->nativeSnapshotEvent.signal();
        };
        if(nativeSnapshotInFlight.ready(mame_machine->save_or_load_pending()))
            finishSnapshot();
        const auto seq=processor->nativeSnapshotRequested.load(std::memory_order_acquire);
        if(seq!=processor->nativeSnapshotFinished.load(std::memory_order_acquire)
           && !nativeSnapshotInFlight.active()
           && !processor->nativeRestore.needsService()
           && nativeSnapshotInFlight.begin(seq,mame_machine->save_or_load_pending(),
                                           bool(nativeRestoreInFlight))) {
            const auto file=processor->nativeSnapshotFile();
            if(file.getParentDirectory().createDirectory().wasOk()
               && (!file.existsAsFile() || file.deleteFile())) {
                // A stale file MUST NOT turn a failed deferred save green.
                mame_machine->immediate_save(file.getFullPathName().toStdString());
                if(nativeSnapshotInFlight.ready(mame_machine->save_or_load_pending()))
                    finishSnapshot();
                // Otherwise MAME has deferred the save. A later update will
                // acknowledge only this generation after the file is written.
            } else {
                // Missing directory / unwritable file: deterministic failure,
                // not a six-second host deadlock or infinite per-frame retry.
                finishSnapshot();
            }
        }

        // --- SYNCHRONIZED MAME STATE SAVING ---
        if (processor->requestMameSave.load(std::memory_order_acquire)
            && akai::machine_state::mayStartSnapshot(
               mame_machine->save_or_load_pending(),bool(nativeRestoreInFlight))
            && !processor->nativeRestore.needsService()) {
        if (saveFrameDelay == 0) { // Only call it the first time
            mame_machine->schedule_save("vst_temp");
            saveFrameDelay = 3;
            }
        }

                if (saveFrameDelay > 0) {
                    saveFrameDelay--;
                    if (saveFrameDelay == 0) {
                        processor->requestMameSave.store(false, std::memory_order_release); // Clear the flag here!
                        processor->mameStateIsReady.store(true, std::memory_order_release);
                        processor->mameStateEvent.signal();
                    }
                }

        // --- SYNCHRONIZED MAME STATE LOADING ---
        if (processor->requestMameLoad.load(std::memory_order_acquire)
            && akai::machine_state::mayStartSnapshot(
               mame_machine->save_or_load_pending(),bool(nativeRestoreInFlight))
            && !processor->nativeRestore.needsService()) {
            if (loadFrameDelay == 0) { // Only call it the first time
                mame_machine->schedule_load("vst_temp");
                loadFrameDelay = 3;
                }
        }

                if (loadFrameDelay > 0) {
                    loadFrameDelay--;
                    if (loadFrameDelay == 0) {
                        processor->requestMameLoad.store(false, std::memory_order_release); // Clear the flag here!
                        processor->mameStateIsReady.store(true, std::memory_order_release);
                        processor->mameStateEvent.signal();
                    }
                }
        
        // --- FLOPPY MOUNTING ---
        if (processor->requestFloppyLoad.exchange(false, std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(processor->mediaMutex);
                    
                    for (device_image_interface &image : image_interface_enumerator(mame_machine->root_device())) {
                        if (image.brief_instance_name() == "flop" || image.brief_instance_name() == "floppydisk") {
                            if (processor->pendingFloppyPath.empty()) {
                                image.unload(); // EJECT!
                            } else {
                                image.load(processor->pendingFloppyPath);
                            }
                            break;
                        }
                    }
        }

        // --- SCSI HARD DISK MOUNTING (scsi:5, brief name "hard"). Fixed drive: mounted at
        //     boot so the firmware's SCSI scan finds it; changing it triggers a reboot. ---
        if (processor->requestHddLoad.exchange(false, std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(processor->mediaMutex);
            for (device_image_interface &image : image_interface_enumerator(mame_machine->root_device())) {
                if (image.brief_instance_name() == std::string("hard")) {
                    if (processor->pendingHddPath.empty()) image.unload();
                    else image.load(processor->pendingHddPath);
                    break;
                }
            }
        }

        // --- SCSI CD-ROM MOUNTING (scsi:4, brief name "cdrm"). Removable: hot-swap. ---
        if (processor->requestCdLoad.exchange(false, std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(processor->mediaMutex);
            for (device_image_interface &image : image_interface_enumerator(mame_machine->root_device())) {
                if (image.brief_instance_name() == std::string("cdrm")) {
                    if (processor->pendingCdPath.empty()) image.unload();
                    else image.load(processor->pendingCdPath);
                    break;
                }
            }
        }

        // --- CARTRIDGE MOUNTING ---
        if (processor->requestCartLoad.exchange(false, std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(processor->mediaMutex);
                    
                    for (device_image_interface &image : image_interface_enumerator(mame_machine->root_device())) {
                        if (image.brief_instance_name() == "cart" || image.brief_instance_name() == "cartridge") {
                            if (processor->pendingCartPath.empty()) {
                                image.unload(); // EJECT!
                            } else {
                                image.load(processor->pendingCartPath);
                            }
                            break;
                        }
                    }
        }
        
        // ==============================================================================
        // --- ASYNCHRONOUS RAM INJECTION & WARM BOOT ---
        // ==============================================================================
        
        if (processor->pendingRamInjection.exchange(false, std::memory_order_acquire)) {
                    
                    auto* osram_share = mame_machine->root_device().memshare("osram");
                    auto* seqram_share = mame_machine->root_device().memshare("seqram");

                    // 1. Inject the OS RAM
                    if (osram_share != nullptr && processor->pendingOsram.getSize() == osram_share->bytes()) {
                            std::memcpy(osram_share->ptr(), processor->pendingOsram.getData(), osram_share->bytes());
                    }
             
                    // 2. Load the Sequencer RAM
                    if (seqram_share != nullptr && processor->pendingSeqRam.getSize() == seqram_share->bytes()) {
                        std::memcpy(seqram_share->ptr(), processor->pendingSeqRam.getData(), seqram_share->bytes());
                    }

                    // 3. Reset the CPU so the SD-1 OS re-evaluates the fresh RAM, RESET SYS-EX in memory
                    device_t* cpu = mame_machine->root_device().subdevice("maincpu");
                    if (cpu != nullptr) {
                            cpu->memory().space(AS_PROGRAM).write_byte(0xFFCE0B, 0x01);
                            cpu->reset();
                    }

                    // 4. Free memory
                    processor->pendingOsram.setSize(0);
                    processor->pendingSeqRam.setSize(0);
                    processor->panicDelaySamples.store(static_cast<int>(processor->getHostSampleRate() * 0.5), std::memory_order_release);
                    
                }
        
        // ==============================================================================
        // --- BANK INJECTION (60-program bank into osram, NO CPU reset) ---
        // ==============================================================================
        // Writes interleaved program data directly into the bank area of osram.
        // Unlike full RAM injection, this does NOT reset the CPU — the firmware
        // picks up the new data when the user presses a Bank button.
        
        if (processor->pendingBankInjection.exchange(false, std::memory_order_acquire)) {
                    auto* osram_share = mame_machine->root_device().memshare("osram");
                    if (osram_share != nullptr && processor->pendingBankData.getSize() > 0) {
                        uint8_t* osram = static_cast<uint8_t*>(osram_share->ptr());
                        const uint8_t* src = static_cast<const uint8_t*>(processor->pendingBankData.getData());
                        size_t len = processor->pendingBankData.getSize();
                        
                        // Motorola 68000 big-endian → LE host byte swap: address ^ 1
                        for (size_t i = 0; i < len; ++i) {
                            osram[(0x0003C8 + i) ^ 1] = src[i];
                        }
                    }
                    processor->pendingBankData.setSize(0);
                }
        
        // Disable on-screen popups (e.g., "State loaded")
        mame_machine->ui().popup_time(0, " ");
        
        // --- FRAME SKIPPING
        frameSkipCounter++;
        if (frameSkipCounter % 2 != 0) {
            return;
        }

        render_target *target = main_target;
        if (target == nullptr) return;
        
        render_primitive_list &prims = target->get_primitives();
        prims.acquire_lock();
        
            // =========================================================================
            // OPTIMIZATION: DIRTY FRAME DETECTION (PRIMITIVE HASHING)
            // =========================================================================
            // Calculate a blazing fast FNV-1a hash of the current MAME UI elements.
            uint64_t currentHash = 14695981039346656037ULL;
            
            for (render_primitive *prim = prims.first(); prim != nullptr; prim = prim->next()) {
                
                // --- NO MOUSE CURSOR (HASH) ---
                bool isMouseCursor = (prim->next() == nullptr && prim->type == render_primitive::QUAD &&
                                      (prim->bounds.x1 - prim->bounds.x0) < 64.0f &&
                                      (prim->bounds.y1 - prim->bounds.y0) < 64.0f);
                
                if (isMouseCursor) continue; 

                auto mix = [&currentHash](const void* data, size_t len) {
                    const uint8_t* p = static_cast<const uint8_t*>(data);
                    for (size_t i = 0; i < len; ++i) {
                        currentHash ^= p[i];
                        currentHash *= 1099511628211ULL;
                    }
                };
                mix(&prim->type, sizeof(prim->type));
                mix(&prim->bounds, sizeof(prim->bounds));
                mix(&prim->color, sizeof(prim->color));
                mix(&prim->texture.base, sizeof(prim->texture.base));
                // LCD LIVE-REFRESH FIX: texture.base is only the ADDRESS of the bitmap. When the
                // LCD content changes (blinking cursor, new text) the address stays the same and
                // only the pixels change, so the hash matched and the frame was dropped -> the
                // panel appeared to refresh "only on events". MAME bumps texture.seqid whenever a
                // texture's CONTENT is updated, so hashing it makes content changes visible.
                mix(&prim->texture.seqid, sizeof(prim->texture.seqid));
            }

                        // If nothing visually changed, and the user didn't resize the VST window,
                        // ABORT the expensive pixel rendering completely!
                        if (currentHash == lastFrameHash && !viewChanged && !sizeChanged) {
                            prims.release_lock();
                            return; // -> GUI CPU USAGE DROPS
                        }
                        
                        lastFrameHash = currentHash;
                        // =========================================================================
        
        // --- DOUBLE BUFFERED RENDERING ---
        // Draw into the buffer that is currently NOT being read by the JUCE GUI thread
        int writeIndex = 1 - processor->readyBufferIndex.load(std::memory_order_acquire);
        juce::Graphics g(processor->screenBuffers[writeIndex]);
        g.fillAll(juce::Colours::black);
        
        for (render_primitive *prim = prims.first(); prim != nullptr; prim = prim->next()) {
                
                // --- FILTER CURSOR ---
                bool isMouseCursor = (prim->next() == nullptr && prim->type == render_primitive::QUAD &&
                                      (prim->bounds.x1 - prim->bounds.x0) < 64.0f &&
                                      (prim->bounds.y1 - prim->bounds.y0) < 64.0f);
                
                if (isMouseCursor) continue; // NO CURSOR

                juce::Rectangle<float> rect(
                                            prim->bounds.x0, prim->bounds.y0,
                                            prim->bounds.x1 - prim->bounds.x0,
                                            prim->bounds.y1 - prim->bounds.y0
                                            );
                    
            if (prim->type == render_primitive::QUAD) {
                if (prim->texture.base != nullptr) {
                    if (prim->texture.width > processor->cachedTexture.getWidth() ||
                        prim->texture.height > processor->cachedTexture.getHeight()) {
                        continue; // Failsafe to prevent out-of-bounds rendering
                    }
                    
                    uint32_t format = PRIMFLAG_GET_TEXFORMAT(prim->flags);
                    
                    // Pre-calculate fixed color multipliers for fast pixel processing
                    const uint32_t rT = (uint32_t)(prim->color.r * 255.0f);
                    const uint32_t gT = (uint32_t)(prim->color.g * 255.0f);
                    const uint32_t bT = (uint32_t)(prim->color.b * 255.0f);
                    const uint32_t aT = (uint32_t)(prim->color.a * 255.0f);
                    
                    const int width = prim->texture.width;
                    const int height = prim->texture.height;
                    const uint32_t srcPitch = prim->texture.rowpixels; 
                    
                    {
                        juce::Image::BitmapData texData(processor->cachedTexture, juce::Image::BitmapData::writeOnly);
                        
                        // 1. ARGB32 MODE (Each pixel has its own Alpha channel)
                        if (format == TEXFORMAT_ARGB32) {
                            for (int y = 0; y < height; ++y) {

                                const uint32_t* __restrict srcRow = static_cast<const uint32_t*>(prim->texture.base) + (y * srcPitch);
                                uint32_t* __restrict dstRow = reinterpret_cast<uint32_t*>(texData.getLinePointer(y));

                                for (int x = 0; x < width; ++x) {
                                    uint32_t p = srcRow[x];
                                    uint32_t a = (p >> 24);
                                    uint32_t r = (p >> 16) & 0xff;
                                    uint32_t g = (p >> 8) & 0xff;
                                    uint32_t b = p & 0xff;

                                    a = (a * aT) >> 8;
                                    r = (((r * rT) >> 8) * a) >> 8;
                                    g = (((g * gT) >> 8) * a) >> 8;
                                    b = (((b * bT) >> 8) * a) >> 8;

                                    dstRow[x] = (a << 24) | (r << 16) | (g << 8) | b;
                                }
                            }
                        }
                        // 2. RGB32 MODE (Alpha is fixed to 255 - Massive optimization!)
                        else if (format == TEXFORMAT_RGB32) {
                            const uint32_t finalA = (255 * aT) >> 8;
                            const uint32_t rMult = (rT * finalA) >> 8;
                            const uint32_t gMult = (gT * finalA) >> 8;
                            const uint32_t bMult = (bT * finalA) >> 8;
                            
                            for (int y = 0; y < height; ++y) {
                                const uint32_t* srcRow = static_cast<const uint32_t*>(prim->texture.base) + (y * srcPitch);
                                uint32_t* dstRow = reinterpret_cast<uint32_t*>(texData.getLinePointer(y));
                                
                                for (int x = 0; x < width; ++x) {
                                    uint32_t p = srcRow[x];
                                    uint32_t r = (p >> 16) & 0xff;
                                    uint32_t g = (p >> 8) & 0xff;
                                    uint32_t b = p & 0xff;
                                    
                                    r = (r * rMult) >> 8;
                                    g = (g * gMult) >> 8;
                                    b = (b * bMult) >> 8;
                                    
                                    dstRow[x] = (finalA << 24) | (r << 16) | (g << 8) | b;
                                }
                            }
                        }
                        // 3. PALETTE16 MODE (MAME's internal palette uses rgb_t, which is 32-bit ARGB)
                        else if (format == TEXFORMAT_PALETTE16) { 
                            const rgb_t* palette = prim->texture.palette;
                            
                            for (int y = 0; y < height; ++y) {
                                const uint16_t* srcRow = static_cast<const uint16_t*>(prim->texture.base) + (y * srcPitch);
                                uint32_t* dstRow = reinterpret_cast<uint32_t*>(texData.getLinePointer(y));
                                
                                for (int x = 0; x < width; ++x) {
                                    uint32_t p = palette[srcRow[x]];
                                    
                                    uint32_t a = 255;
                                    uint32_t r = (p >> 16) & 0xff;
                                    uint32_t g = (p >> 8) & 0xff;
                                    uint32_t b = p & 0xff;
                                    
                                    a = (a * aT) >> 8;
                                    r = (((r * rT) >> 8) * a) >> 8;
                                    g = (((g * gT) >> 8) * a) >> 8;
                                    b = (((b * bT) >> 8) * a) >> 8;
                                    
                                    dstRow[x] = (a << 24) | (r << 16) | (g << 8) | b;
                                }
                            }
                        }
                    } // Scoped BitmapData write lock released here
                    
                    g.drawImage(processor->cachedTexture,
                                static_cast<int>(rect.getX()), static_cast<int>(rect.getY()),
                                static_cast<int>(rect.getWidth()), static_cast<int>(rect.getHeight()),
                                0, 0, width, height, false);
                    
                } else {
                    juce::Colour color((uint8_t)(prim->color.r * 255.0f), (uint8_t)(prim->color.g * 255.0f),
                                       (uint8_t)(prim->color.b * 255.0f), (uint8_t)(prim->color.a * 255.0f));
                    g.setColour(color);
                    g.fillRect(rect);
                }
                
            } else if (prim->type == render_primitive::LINE) {
                juce::Colour color((uint8_t)(prim->color.r * 255.0f), (uint8_t)(prim->color.g * 255.0f),
                                   (uint8_t)(prim->color.b * 255.0f), (uint8_t)(prim->color.a * 255.0f));
                g.setColour(color);
                
                g.drawLine(prim->bounds.x0, prim->bounds.y0, prim->bounds.x1, prim->bounds.y1, prim->width);
            }
        }
        
        prims.release_lock();
                            
        // Swap the ready buffer index. The JUCE GUI will now pick up the fresh frame.
        processor->readyBufferIndex.store(writeIndex, std::memory_order_release);
        processor->getFrameFlag().store(true, std::memory_order_release);
    }
        
    virtual void input_update(bool relative_reset) override {
        
            if (mame_machine == nullptr) return;
            

            // P004: safe original-Akai firmware recording operator.
            // This is the same owner thread as ioport_field::set_value.
            // No handwritten register manipulation, no blind timing macros.
            {
                const double t=mame_machine->time().as_double();
                const std::uint64_t nowMs=(std::isfinite(t)&&t>=0.0)
                    ? static_cast<std::uint64_t>(t*1000.0):0;
                if(processor->autoCaptureCancel.exchange(false,std::memory_order_acq_rel)) {
                    const auto action=captureController.cancel(nowMs);
                    if(captureHeldIndex>=0 &&
                       captureHeldIndex<int(processor->buttonParams.size())) {
                        processor->buttonParams[std::size_t(captureHeldIndex)]->store(
                            0.0f,std::memory_order_release);
                        captureHeldIndex=-1;
                    }
                    processor->autoCaptureStage.store(int(action.stage),std::memory_order_release);
                }
                if(processor->autoCaptureRequest.exchange(false,std::memory_order_acq_rel)) {
                    if(captureController.start(nowMs,
                         processor->wavTargetRecordCentis.load(
                             std::memory_order_acquire)))
                        processor->autoCaptureStage.store(
                            int(captureController.stage()),std::memory_order_release);
                }
                if(captureController.isRunning()) {
                    char lcd[321]{};
                    auto* display=mame_machine->root_device().subdevice<hd61830_device>("lcdc");
                    const bool valid=display!=nullptr &&
                        display->snapshot_visible_text(lcd,sizeof(lcd));
                    akai::capture::Observe obs;
                    obs.readable=valid;
                    obs.lcd=valid?std::string_view(lcd,320):std::string_view{};
                    obs.wavReady=processor->wavAcceptedByEngine();
                    obs.wavComplete=processor->wavState.load(std::memory_order_acquire)==3;
                    obs.wavSourcePresent=processor->wavSourcePresent();
                    obs.firmwareReady=processor->mameIsFullyBooted.load(
                        std::memory_order_acquire);
                    obs.wadcsKnown=processor->firmwareWadcsKnown.load(
                        std::memory_order_acquire);
                    obs.wadcs=processor->firmwareWadcsLatch.load(
                        std::memory_order_acquire);
                    const auto action=captureController.step(obs,nowMs);
                    auto findIndex=[&](akai::capture::Button key) {
                        const char* tag=nullptr;
                        switch(key) {
                        case akai::capture::Button::SelectSingle:tag="btn_single";break;
                        case akai::capture::Button::EditProgram:tag="btn_edit";break;
                        case akai::capture::Button::KeygroupsF2:tag="btn_f2";break;
                        case akai::capture::Button::SamplePageF6:tag="btn_f6";break;
                        case akai::capture::Button::Sample:tag="btn_sample";break;
                        case akai::capture::Button::RecF2:tag="btn_f2";break;
                        case akai::capture::Button::ArmF8:tag="btn_f8";break;
                        case akai::capture::Button::GoF7:tag="btn_f7";break;
                        case akai::capture::Button::Name:tag="btn_name";break;
                        case akai::capture::Button::Enter:tag="btn_enter";break;
                        case akai::capture::Button::CursorDown:tag="btn_down";break;
                        case akai::capture::Button::CursorRight:tag="btn_right";break;
                        case akai::capture::Button::Digit0:tag="btn_key0";break;
                        case akai::capture::Button::Digit1:tag="btn_key1";break;
                        case akai::capture::Button::Digit2:tag="btn_key2";break;
                        case akai::capture::Button::Digit3:tag="btn_key3";break;
                        case akai::capture::Button::Digit4:tag="btn_key4";break;
                        case akai::capture::Button::Digit5:tag="btn_key5";break;
                        case akai::capture::Button::Digit6:tag="btn_key6";break;
                        case akai::capture::Button::Digit7:tag="btn_key7";break;
                        case akai::capture::Button::Digit8:tag="btn_key8";break;
                        case akai::capture::Button::Digit9:tag="btn_key9";break;
                        default:return -1;
                        }
                        for(size_t n=0;n<processor->sd1Buttons.size();++n)
                            if(processor->sd1Buttons[n].paramID==tag)return int(n);
                        return -1;
                    };
                    const int keyIndex=findIndex(action.button);
                    if(action.command==akai::capture::Command::Press) {
                        if(keyIndex>=0&&keyIndex<int(processor->buttonParams.size()) &&
                           processor->buttonParams[std::size_t(keyIndex)]!=nullptr &&
                           processor->buttonParams[std::size_t(keyIndex)]->load(
                               std::memory_order_relaxed)<=0.5f) {
                            processor->buttonParams[std::size_t(keyIndex)]->store(
                                1.0f,std::memory_order_release);
                            captureHeldIndex=keyIndex;
                        } else {
                            // A user/host already holds this key, or mapping
                            // is absent. Never steal/release someone else's key.
                            captureController.cancel(nowMs);
                            processor->autoCaptureStage.store(
                                int(akai::capture::Stage::Busy),std::memory_order_release);
                        }
                    } else if(action.command==akai::capture::Command::Release) {
                        if(captureHeldIndex>=0 &&
                           captureHeldIndex<int(processor->buttonParams.size())) {
                            processor->buttonParams[std::size_t(captureHeldIndex)]->store(
                                0.0f,std::memory_order_release);
                            captureHeldIndex=-1;
                        }
                    }
                    if(action.command!=akai::capture::Command::Press ||
                       captureHeldIndex>=0)
                        processor->autoCaptureStage.store(
                            int(captureController.stage()),std::memory_order_release);
                }
            }
            // P030 original Akai MIDI receive. MAME owner thread invokes the real
            // midiin image device, which serializes each genuine SDS SysEx packet
            // through the Akai UART. This uses original firmware to create PCM.
            {
                const double t=mame_machine->time().as_double();
                const std::uint64_t now=(std::isfinite(t)&&t>=0.0)
                    ?std::uint64_t(t*1000.0):0;
                auto* in=mame_machine->root_device().subdevice<midiin_device>(
                    "mdin:midiin:midiinimg");
                if(originalSdsTransferActive && now>=originalSdsFinishAtMs) {
                    originalSdsTransferActive=false;
                    // The actual input image's UART stream must be allowed to
                    // drain before restoring DAW live MIDI. This is not an
                    // assertion that original firmware assigned a program.
                    if(in) {
                        const auto restored=in->load("VST MIDI");
                        processor->originalSdsStage.store(
                            restored.first ? 4:3,std::memory_order_release);
                    } else processor->originalSdsStage.store(
                        4,std::memory_order_release);
                }
                if(!originalSdsTransferActive &&
                   processor->originalSdsRequested.load(std::memory_order_acquire) &&
                   !captureController.isRunning() &&
                   !nativeRestoreInFlight && !nativeSnapshotInFlight.active() &&
                   !mame_machine->save_or_load_pending()) {
                    std::string staged;
                    std::uint64_t duration=0;
                    {
                        std::lock_guard<std::mutex> lock(processor->originalSdsMutex);
                        staged=processor->originalSdsPendingPath;
                        duration=processor->originalSdsPendingDurationMs;
                        processor->originalSdsRequested.store(
                            false,std::memory_order_release);
                    }
                    if(in && !staged.empty() && duration>0) {
                        const auto status=in->load(staged);
                        if(!status.first) {
                            originalSdsTransferActive=true;
                            originalSdsFinishAtMs=now+duration;
                            processor->originalSdsStage.store(
                                2,std::memory_order_release);
                        } else {
                            processor->originalSdsStage.store(
                                4,std::memory_order_release);
                            (void)in->load("VST MIDI");
                        }
                    } else processor->originalSdsStage.store(
                        4,std::memory_order_release);
                }
            }
            // ==============================================================================
            // --- 1. MOUSE INJECTION ---
            // ==============================================================================
            render_target* target = main_target;
            if (target != nullptr) {
                int x = processor->mouseX.load(std::memory_order_relaxed);
                int y = processor->mouseY.load(std::memory_order_relaxed);
                uint32_t currentBtns = processor->mouseButtons.load(std::memory_order_relaxed);
                
                int32_t pressed =  ((currentBtns & 1) && !(lastMouseButtons & 1)) ? 1 : 0;
                int32_t released = (!(currentBtns & 1) && (lastMouseButtons & 1)) ? 1 : 0;
                int32_t clicks = pressed;

                // S3000XL / MAME 0.288: the ui_input pointer API was refactored (push_pointer_update
                // / pointer::MOUSE removed). Mouse-pointer injection is disabled; panel buttons &
                // pads use the layout hit-test -> buttonParams (set_value) path instead.
                juce::ignoreUnused(target, x, y, currentBtns, pressed, released, clicks);
                
                lastMouseButtons = currentBtns;
            }

                    // ==============================================================================
                    // --- SAVE PROGRAM MACRO: bank detection + electronic hold (audio thread) ---
                    // ==============================================================================
                    // S3000XL PORT: the SD-1 "Save Program" macro relied on the Ensoniq
                    // esqpanel2x40_vfx_device ("panel"), which is NOT part of the s3000xl
                    // machine and is NOT compiled into an s3000xl-only MAME build. Removing
                    // the esqpanel dependency here (otherwise ESQPANEL2X40_VFX is an
                    // unresolved external at link time). The macro is not needed on the MPC.
                    audioMacroHeldBank = -1;
                    processor->detectedBankMask.store(0, std::memory_order_release);

                    // ==============================================================================
                    // --- 2. VST BUTTON AUTOMATION (Direct Hardware Memory Injection) ---
                    // ==============================================================================
                    // We bypass the mouse completely and directly pull the circuits on the MAME motherboard!
                    bool macroActive = processor->isSaveMacroActive.load(std::memory_order_relaxed);
                    for (size_t i = 0; i < processor->sd1Buttons.size(); ++i) {
                        
                        // Lock-free read from the DAW's automation lane
                        float val = processor->buttonParams[i]->load(std::memory_order_relaxed);
                        bool isPressed = (val > 0.5f);

                        // DATA ENTRY dial is analog -> handled after this loop (skip digital path).
                        if (juce::String(processor->sd1Buttons[i].ioportTag) == ":DATAENTRY")
                            continue;

                        // During the Save Program macro, don't touch bank buttons (indices 10-19).
                        // They are controlled by MAME UI (mouse → PORT_CHANGED → set_button) for
                        // detection, and by our set_button hold above. The APVTS set_value(0) would
                        // fire PORT_CHANGED(false) and break both.
                        if (macroActive && i >= 10 && i <= 19 && !isPressed)
                            continue;

                        // PORT CALLING
                        ioport_port* port = mame_machine->root_device().ioport(processor->sd1Buttons[i].ioportTag);
                        
                        if (port != nullptr) {
                            // Locate the exact button on the circuit board using its hex mask
                            ioport_field* field = port->field(processor->sd1Buttons[i].ioportMask);
                            if (field != nullptr) {
                                // Electronically press or release the button!
                                field->set_value(isPressed ? 1 : 0);
                            }
                        }
                    }

                    // --- DATA ENTRY DIAL (analog): dialValue is driven by the editor (click = 1
                    // step, drag = turn) and by the "data_entry" VST param; here we just apply it.
                    {
                        // ENDLESS encoder: the position wraps 255 -> 0 and 0 -> 255 instead of
                        // hitting a stop, matching the hardware knob (the firmware reads the
                        // absolute value and derives the rotation from the delta).
                        const int dv = processor->dialValue.load(std::memory_order_relaxed) & 0xFF;
                        ioport_port* dport = mame_machine->root_device().ioport(":DATAENTRY");
                        if (dport != nullptr) {
                            ioport_field* dfield = dport->field(0xff);
                            if (dfield != nullptr && dfield->live().analog != nullptr)
                                dfield->live().analog->set_value(dv);
                        }
                    }
        }
    
    virtual void check_osd_inputs() override {};
    virtual void set_verbose(bool print_verbose) override {};

    virtual void init_debugger() override {};
    virtual void wait_for_debugger(device_t &device, bool firststop) override {};

    virtual bool no_sound() override { return false; };
    virtual bool sound_external_per_channel_volume() override { return false; };
    virtual bool sound_split_streams_per_source() override { return false; };
            
    virtual osd::audio_info sound_get_information() override {
        osd::audio_info info;
        osd::audio_info::node_info node;
        
        node.m_name = "vst_audio";
        node.m_display_name = "VST Audio Output";
        node.m_id = 1;
        
        // Force MAME to generate audio at the exact sample rate required by the DAW host.
        node.m_rate = { static_cast<uint32_t>(processor->getHostSampleRate()) };
        node.m_sinks = 1;
        // [SAMPLING] The node now also offers a SOURCE - the sampler input. Without this MAME has
        // nowhere to pull recording audio from, so a MICROPHONE device in the driver would read
        // nothing however well the rest of the chain is built.
        node.m_sources = EnsoniqSD1AudioProcessor::SAMPLER_IN_CHANNELS;

        // [SAMPLING] THE MISSING LINK, and it took a run to find because nothing reports it.
        //
        // MAME matches a sound_io device's channels to an OSD node's channels BY POSITION:
        //     sound.cpp:2478  find_channel_mapping(omap.m_dev->get_position(channel), node)
        //     sound.cpp:2064      for(u32 port = 0; port != node->m_port_positions.size(); port++)
        // With m_port_positions EMPTY there are no candidates, so no mixing step is built, and
        // input_get() then walks an empty list and pulls nothing - silently. The stream is opened, the
        // machine reads the A/D 352 800 times, and not one sample is ever requested from us.
        //
        // The output side never needed this because this plugin takes its audio out through
        // add_audio_to_recording rather than the sink stream, so the omission could not show itself
        // until an input existed.
        node.m_port_positions.push_back(osd::channel_position::FL());
        node.m_port_positions.push_back(osd::channel_position::FR());

        info.m_nodes.push_back(node);
        info.m_default_sink = 1;
        info.m_default_source = 1;
        info.m_generation = 1;

        return info;
    };

    virtual uint32_t sound_stream_sink_open(uint32_t node, std::string name, uint32_t rate) override { return 1; };
    virtual void sound_stream_close(uint32_t id) override {};
    
    virtual void add_audio_to_recording(const int16_t *buffer, int samples_this_frame) override {
        if (processor != nullptr) {
            processor->pushAudioFromMame(buffer, samples_this_frame);
        }
    };
    
    // [SAMPLING] 0 means "refused" in this API, which is what this returned before - so MAME never
    // opened an input stream at all. Id 2 keeps it distinct from the sink's id 1.
    virtual uint32_t sound_stream_source_open(uint32_t node, std::string name, uint32_t rate) override {
        // [SAMPLING] logerror, not osd_printf_verbose - the capture hooks logerror, and a message that
        // goes to a channel nobody reads is how a whole round was lost earlier.
        if (mame_machine != nullptr)
            if (VST_TRACE) mame_machine->logerror("SAMPLER-IN stream OPENED (node %u, rate %u) - MAME is now pulling audio\n",
                                   node, rate);
        return 2;
    };
    virtual uint32_t sound_get_generation() override { return 1; };

    // [SAMPLING] MAME pulls the sampler input here, interleaved int16, SAMPLER_IN_CHANNELS per frame.
    // Anything we cannot supply comes back as silence - see pushAudioToMame for why that is the honest
    // answer rather than a fudge.
    virtual void sound_stream_source_update(uint32_t id, int16_t *buffer, int samples_this_frame) override {
        if (processor != nullptr && buffer != nullptr && samples_this_frame > 0)
            processor->pullAudioForMame(buffer, samples_this_frame);
        else if (buffer != nullptr && samples_this_frame > 0)
            std::fill_n(buffer, samples_this_frame * EnsoniqSD1AudioProcessor::SAMPLER_IN_CHANNELS, int16_t(0));
    };
    virtual void sound_stream_set_volumes(uint32_t id, const std::vector<float> &db) override {};
    virtual void sound_begin_update() override {};
    virtual void sound_end_update() override {};
    virtual void sound_stream_sink_update(uint32_t id, const int16_t *buffer, int samples_this_frame) override {};

    virtual void customize_input_type_list(std::vector<input_type_entry> &typelist) override { typelist.clear(); };
    virtual std::vector<ui::menu_item> get_slider_list() override { return {}; };

    virtual osd_font::ptr font_alloc() override { return nullptr; };
    virtual bool get_font_families(std::string const &font_path, std::vector<std::pair<std::string, std::string> > &result) override { return false; };
    virtual bool execute_command(const char *command) override { return false; };

    virtual std::unique_ptr<osd::midi_input_port> create_midi_input(std::string_view name) override {
        return std::make_unique<VstMidiInputPort>(processor);
    };
    
    virtual std::unique_ptr<osd::midi_output_port> create_midi_output(std::string_view name) override {
        return std::make_unique<VstMidiOutputPort>(processor);
    };

    virtual std::vector<osd::midi_port_info> list_midi_ports() override {
        std::vector<osd::midi_port_info> ports;
        osd::midi_port_info info;
        
        info.name = "VST MIDI"; 
        info.input = true;
        info.output = true;
        info.default_input = true;
        info.default_output = true;
        
        ports.push_back(info);
        return ports;
    };

    virtual std::unique_ptr<osd::network_device> open_network_device(int id, osd::network_handler &handler) override { return {}; };
    virtual std::vector<osd::network_device_info> list_network_devices() override { return {}; };
};

// ==============================================================================
// VST AUTOMATION (APVTS) DEFINITIONS
// ==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EnsoniqSD1AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Automated parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("volume", "Volume", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("data_entry", "Data Entry", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pitch_bend", "Pitch Bend", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mod_wheel", "Mod Wheel", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sustain_pedal", "Sustain Pedal", 0.0f, 1.0f, 0.0f));
    
    // --- AUTOMATED BUTTONS (Generated dynamically from sd1Buttons array) ---
    for (const auto& btn : sd1Buttons) {
            params.push_back(std::make_unique<juce::AudioParameterBool>(btn.paramID, btn.paramName, false));
        }
        
    // --- No automation of settings ---
    auto nonAutomatable = juce::AudioParameterChoiceAttributes().withAutomatable(false);

    juce::StringArray bufferSizes = { "128", "256", "512", "1024", "2048", "4096", "8192" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("buffer_size", 1), "Internal Buffer", bufferSizes, 2, nonAutomatable));

    // Dynamic Panel Layout Selector
    juce::StringArray views = { "Compact (Default)", "Full Keyboard", "Rack Panel", "Tablet View" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("layout_view", 1), "Panel Layout", views, 0, nonAutomatable));

    // Preserve all existing parameter IDs/ordering: legacy "volume" is MIDI CC7.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "rec_gain", "REC GAIN", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "main_volume", "MAIN VOLUME", 0.0f, 1.0f, 1.0f));
    return { params.begin(), params.end() };
}

void EnsoniqSD1AudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
        
    // --- 1. SETUP ---
    if (parameterID == "buffer_size") {
                auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("buffer_size"));
                if (choiceParam != nullptr) {
                    int sizes[] = { 128, 256, 512, 1024, 2048, 4096, 8192 };
                    int newThreshold = sizes[choiceParam->getIndex()];
                    mameBufferThreshold.store(newThreshold, std::memory_order_relaxed);
                    
                    if ((juce::MessageManager::getInstanceWithoutCreating() != nullptr && juce::MessageManager::getInstanceWithoutCreating()->isThisTheMessageThread())) {
                        setLatencySamples(newThreshold + getInternalHardwareLatencySamples());
                    }
                    // NEW: AU audio-thread fallback
                    else if (wrapperType == juce::AudioProcessor::wrapperType_AudioUnit) {
                        juce::MessageManager::callAsync([this, newThreshold]() {
                            setLatencySamples(newThreshold + getInternalHardwareLatencySamples());
                        });
                    }
                }
                requestGlobalSave.store(true, std::memory_order_release);
            }
                
    else if (parameterID == "layout_view") {
            auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("layout_view"));
            int idx = choiceParam != nullptr ? choiceParam->getIndex() : 0;
            
            requestedViewIndex.store(idx, std::memory_order_release);
            requestViewChange.store(true, std::memory_order_release);
            requestGlobalSave.store(true, std::memory_order_release);
        }
    
        // --- 2. AUTOMATION ---
        else {
            uint64_t targetSample = totalRead.load(std::memory_order_acquire) + mameBufferThreshold.load(std::memory_order_relaxed);
            double sr = hostSampleRate.load(std::memory_order_relaxed);
            double t_anchor = anchorMameTime.load(std::memory_order_relaxed);
            uint64_t s_anchor = anchorDawSample.load(std::memory_order_relaxed);
                        
            // Same clean math as in processBlock
            double targetMameTime = t_anchor + static_cast<double>(targetSample - s_anchor) / sr;

            if (parameterID == "volume") {
                uint8_t val = static_cast<uint8_t>(newValue * 127.0f);
                pushMidiByte(0xB0, targetMameTime);
                pushMidiByte(0x07, targetMameTime);
                pushMidiByte(val, targetMameTime);
            }
            else if (parameterID == "data_entry") {
                // DAW "Data Entry" slider (0..1) -> S3000XL data-entry dial (0..255).
                dialValue.store(((int)(newValue * 255.0f)) & 0xFF, std::memory_order_relaxed);
            }
        }
}
// ==============================================================================

void EnsoniqSD1AudioProcessor::loadGlobalSettings()
{
    juce::InterProcessLock settingsLock("S3000XL_Settings_Lock");
    if (!settingsLock.enter(500)) return;  // wait up to 500ms for another instance to finish writing

    juce::File docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    juce::File settingsFile = docsDir.getChildFile("S3000XL").getChildFile("settings.xml");

    for (int i = 0; i < 5; ++i) {
        if (settingsFile.existsAsFile()) {
            if (auto xml = juce::XmlDocument::parse(settingsFile)) {
                
                int bufIdx = xml->getIntAttribute("buffer_size", 2);
                int viewIdx = xml->getIntAttribute("layout_view", 0);
                savedWindowWidth = xml->getIntAttribute("window_width", 1200);
                savedWindowHeight = xml->getIntAttribute("window_height", 900);

                fileManagerState.fmWindowWidth = xml->getIntAttribute("fm_window_width", 1200);
                fileManagerState.fmWindowHeight = xml->getIntAttribute("fm_window_height", 925);

                showWelcomeMessage.store(!xml->getBoolAttribute("welcome_shown", false), std::memory_order_release);
                customRomPath = xml->getStringAttribute("rom_path", "");

                // S3000XL SCSI media paths (persisted globally)
                {
                    std::lock_guard<std::mutex> lock(mediaMutex);
                    pendingHddPath = xml->getStringAttribute("scsi_hdd_path", "").toStdString();
                    pendingCdPath  = xml->getStringAttribute("scsi_cd_path",  "").toStdString();
                }
                isHddLoaded.store(!pendingHddPath.empty(), std::memory_order_release);
                isCdLoaded.store(!pendingCdPath.empty(),  std::memory_order_release);
                loadedHddName = juce::File(juce::String(pendingHddPath)).getFileName();
                loadedCdName  = juce::File(juce::String(pendingCdPath)).getFileName();

                lastBrowsedFolder = xml->getStringAttribute("last_browsed_folder");
                lastMediaFolder   = xml->getStringAttribute("last_media_folder");
                lastRomFolder     = xml->getStringAttribute("last_rom_folder");
                myComputerPath    = xml->getStringAttribute("my_computer_path");

                bookmarkFolders.clear();
                for (int j = 0; j < 10; ++j) {
                    juce::String key = "bookmark_" + juce::String(j);
                    juce::String path = xml->getStringAttribute(key, "");
                    if (path.isNotEmpty())
                        bookmarkFolders.add(path);
                }

                if (auto* p = apvts.getParameter("buffer_size"))
                    p->setValue(p->convertTo0to1(bufIdx));

                if (auto* p = apvts.getParameter("layout_view"))
                    p->setValue(p->convertTo0to1(viewIdx));

                int sizes[] = { 128, 256, 512, 1024, 2048, 4096, 8192 };
                if (bufIdx >= 0 && bufIdx < 6) {
                    mameBufferThreshold.store(sizes[bufIdx], std::memory_order_relaxed);
                }
                requestedViewIndex.store(viewIdx, std::memory_order_release);
                
                settingsLock.exit();
                return;
            }
        } else {
            showWelcomeMessage.store(true, std::memory_order_release);
            settingsLock.exit();
            return; 
        }
        juce::Thread::sleep(10);
    }
    settingsLock.exit();
}

EnsoniqSD1AudioProcessor::EnsoniqSD1AudioProcessor()
     : AudioProcessor (BusesProperties()
                       // [SAMPLING] The sampler input, DEFAULT ON.
                       //
                       // It shipped default-OFF to keep existing sessions untouched, and that was the
                       // wrong trade: with the bus disabled getTotalNumInputChannels() returns 0,
                       // pushAudioToMame returns immediately, and the ring stays empty no matter what
                       // the host is wired to. Thor patched a TB-303 straight into the plugin and the
                       // machine still recorded silence, because the plugin was not asking for audio at
                       // all. This is a SAMPLER; its input is not an optional extra, it is the
                       // instrument. isBusesLayoutSupported still accepts disabled, so a host that does
                       // not want it can still turn it off.
                       .withInput  ("Sampler In", juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Main Out",   juce::AudioChannelSet::stereo(), true)
                       // [MULTIOUT 4] The S3000XL's eight individual outputs, as eight MONO buses.
                       // Enabled by default so a host shows them without the user having to hunt; a
                       // host that wants only the stereo pair can still disable them, which
                       // isBusesLayoutSupported accepts.
                       .withOutput ("OUT 1", juce::AudioChannelSet::mono(), true)
                       .withOutput ("OUT 2", juce::AudioChannelSet::mono(), true)
                       .withOutput ("OUT 3", juce::AudioChannelSet::mono(), true)
                       .withOutput ("OUT 4", juce::AudioChannelSet::mono(), true)
                       .withOutput ("OUT 5", juce::AudioChannelSet::mono(), true)
                       .withOutput ("OUT 6", juce::AudioChannelSet::mono(), true)
                       .withOutput ("OUT 7", juce::AudioChannelSet::mono(), true)
                       .withOutput ("OUT 8", juce::AudioChannelSet::mono(), true)
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Generate a unique sandbox directory for this instance's NVRAM/CFG
    instanceTempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("S3000XL_Instance_" + juce::Uuid().toString())
                            .getFullPathName();
    
    recGainParameter = apvts.getRawParameterValue("rec_gain");
    mainVolumeParameter = apvts.getRawParameterValue("main_volume");
    jassert(recGainParameter != nullptr && mainVolumeParameter != nullptr);
    apvts.addParameterListener("volume", this);
    apvts.addParameterListener("data_entry", this);
    apvts.addParameterListener("pitch_bend", this);
    apvts.addParameterListener("mod_wheel", this);
    apvts.addParameterListener("buffer_size", this);
    apvts.addParameterListener("layout_view", this);
    
    // Initialize VFD array to 0 (blank segments)
    for (int i = 0; i < VFD_SIZE; ++i) {
        vfdSegments[i].store(0, std::memory_order_relaxed);
    }
        
    // Build the VFD text dictionary from the ROM file
    buildVfdDictionary();
    
    // --- CACHE VST BUTTON POINTERS FOR MAME (0% CPU overhead) ---
    // We store the raw atomic pointers so the MAME audio thread can read them instantly
    // without locking or string lookups.
    for (const auto& btn : sd1Buttons) {
        buttonParams.push_back(apvts.getRawParameterValue(btn.paramID));
    }
    
    // Loading global settings at start
    loadGlobalSettings();
    
#ifdef _WIN32
        // Request 1ms timer resolution from the Windows OS scheduler.
        // By default, Windows thread sleeping (e.g., wait(1)) can overshoot up to 15.6ms.
        // This strict 1ms resolution ensures the MAME background thread wakes up precisely,
        // preventing audio dropouts during low-latency real-time playback, while keeping
        // offline rendering (bounce) mathematically intact and fully synchronized.
        timeBeginPeriod(1);
#endif
}

EnsoniqSD1AudioProcessor::~EnsoniqSD1AudioProcessor()
{
        // safely shut down MAME engine
        shutdownMame();

#ifdef _WIN32
        // Release the high-resolution timer request gracefully when the plugin is destroyed
        // or removed from the DAW track, returning Windows to its default scheduler resolution.
        timeEndPeriod(1);
#endif
}

void EnsoniqSD1AudioProcessor::shutdownMame()
{
        isMameRunning.store(false, std::memory_order_release);
                
        mameThrottleEvent.signal();
        mameStateEvent.signal();
        // Abort any host waiting on this particular MAME instance.
        nativeSnapshotSucceeded.store(false,std::memory_order_release);
        nativeSnapshotEvent.signal();
        
        if (mameThread.joinable()) {
            mameThread.join();
        }
       
        // --- SYNC NVRAM TO MASTER AND CLEANUP SANDBOX ---
        juce::File instDir(instanceTempDir);
        if (instDir.exists()) {
            juce::InterProcessLock nvramLock("S3000XL_NVRAM_Lock");
            
            // Wait up to 2 seconds for another instance to finish saving
            if (nvramLock.enter(2000)) {
                juce::File masterDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                        .getChildFile("S3000XL").getChildFile("GlobalState");
                masterDir.createDirectory();
                
                juce::File instNvram = instDir.getChildFile("nvram").getChildFile("s3000xl");
                juce::File masterNvram = masterDir.getChildFile("nvram").getChildFile("s3000xl");
                
                if (instNvram.exists() && instNvram.isDirectory()) {
                    masterNvram.createDirectory();
                    juce::File instOsram = instNvram.getChildFile("osram");
                    juce::File instSeqram = instNvram.getChildFile("seqram");
                    juce::File destOsram = masterNvram.getChildFile("osram");
                    juce::File destSeqram = masterNvram.getChildFile("seqram");
                    
                    if (instOsram.existsAsFile()) {
                        destOsram.deleteFile();
                        instOsram.copyFileTo(destOsram);
                    }
                    if (instSeqram.existsAsFile()) {
                        destSeqram.deleteFile();
                        instSeqram.copyFileTo(destSeqram);
                    }
                }
                nvramLock.exit();
            }
            // Delete the unique temp directory completely
            instDir.deleteRecursively();
        }

        mameHasStarted.store(false, std::memory_order_release);
}

//==============================================================================
const juce::String EnsoniqSD1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EnsoniqSD1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EnsoniqSD1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EnsoniqSD1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EnsoniqSD1AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EnsoniqSD1AudioProcessor::getNumPrograms()
{
    return 1;
}

int EnsoniqSD1AudioProcessor::getCurrentProgram()
{
    return 0;
}

void EnsoniqSD1AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EnsoniqSD1AudioProcessor::getProgramName (int index)
{
    // JUCE exports one VST3 host program placeholder. An empty name violates
    // Steinberg's Scan Programs test, even before original Akai ROM boot.
    // This is NOT a guest Akai program/keygroup and never changes guest state.
    return index == 0 ? juce::String("Akai S3000XL") : juce::String();
}

void EnsoniqSD1AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EnsoniqSD1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{

    // AU anchor reset
#if JucePlugin_Build_AU
    auAnchorSet.store(false, std::memory_order_release);
#endif
        
    juce::File docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    juce::File ensoniqDir = docsDir.getChildFile("S3000XL");
    if (!ensoniqDir.exists()) ensoniqDir.createDirectory();

    hostSampleRate.store(sampleRate);
    // Apply recalled analogue levels immediately at host activation; only live
    // knob movements are smoothed. This prevents stale startup level surprises.
    smoothedRecGain = recGainParameter != nullptr
        ? AkaiAnalogControls::normalized(recGainParameter->load(std::memory_order_relaxed)) : 1.0f;
    smoothedMainVolume = mainVolumeParameter != nullptr
        ? AkaiAnalogControls::normalized(mainVolumeParameter->load(std::memory_order_relaxed)) : 1.0f;
    smoothedMonitorAmount = 0.0f; // reset crossfade on host reprepare
    
    auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("buffer_size"));
        if (choiceParam != nullptr) {
            int sizes[] = { 128, 256, 512, 1024, 2048, 4096, 8192 };
            mameBufferThreshold.store(sizes[choiceParam->getIndex()], std::memory_order_relaxed);
        }

        // Report buffer for Latency Compensation (PDC)
        int currentThreshold = mameBufferThreshold.load(std::memory_order_relaxed);
        int hwLatency = getInternalHardwareLatencySamples();
        
        setLatencySamples(currentThreshold + hwLatency);
    
        // Only boot MAME the very first time play is prepared
        if (!mameHasStarted.exchange(true)) {
            
                        // --- VST SCANNER PROTECTION ---
                        // We check which process is currently running the plugin
                        juce::String hostPath = juce::PluginHostType().getHostPath().toLowerCase();
                        
                        // Detect Maschine once, shared by all processBlock Maschine-specific branches
                        isMaschineHost = hostPath.contains("maschine");
                        
                        // If this is a plugin scanner or validator (e.g., Cubase vstscanner.exe),
                        // we have absolutely no intention of running MAME and writing to the file system!
                        if (hostPath.contains("scanner") || hostPath.contains("validator")) {
                            isMameRunning = false;
                            return;
                        }
        
        initialSampleRate.store(sampleRate);
            
                        // 1. Run rigorous Self-Check
                        if (runSelfCheck()) {
                        // 2. If healthy, proceed to ROM check and MAME boot
                            checkRomAndBootMame();
                        } else {
                            // Self-check failed, halt completely
                            isMameRunning = false;
                        }
        
    } else {
        
        // MAME cannot change sample rates on the fly. If the DAW changes it mid-session, we must halt processing.
        if (sampleRate != initialSampleRate.load()) {
            sampleRateMismatch.store(true, std::memory_order_release); 
        } else {
            sampleRateMismatch.store(false, std::memory_order_release); 
        }
    }
    
        // --- Buffer reset & PDC Pre-fill ---
        totalRead.store(0, std::memory_order_release);
        
        // We shift the MAME write pointer forward by the specified PDC delay!
        // This ensures that the first 'currentThreshold' samples will be pure silence,
        // which the DAW's PDC will be able to trim precisely from the beginning of the file.

        // We shift the MAME write pointer forward by the specified PDC delay!
        totalWritten.store(currentThreshold, std::memory_order_release);
        
        // --- Reset interpolator ---
        needAnchorSync.store(true, std::memory_order_release);
            
        midiReadPos.store(0, std::memory_order_release);
        midiWritePos.store(0, std::memory_order_release);
    
#if JucePlugin_Build_AU
    pendingAUMidi.clear();
#endif

        // We clear the entire ring buffer to ensure that the preloaded section is completely silent
        for (int i = 0; i < RING_BUFFER_SIZE; ++i) {
            ringBufferL[i] = 0.0f;
            ringBufferR[i] = 0.0f;
            for (int c = 0; c < INDIV_OUTS; ++c) ringBufferIndiv[c][i] = 0.0f;
        }
    
                // NEW: AU Flag that prepareToPlay just finished
                if (wrapperType == juce::AudioProcessor::wrapperType_AudioUnit) {
                    prepareWasCalled.store(true, std::memory_order_release);
                    maxOfflineBuffer.store(mameBufferThreshold.load(std::memory_order_relaxed), std::memory_order_relaxed);
                }
}

void EnsoniqSD1AudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EnsoniqSD1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // 1. Main Out MUST be active and stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // 2. [MULTIOUT 4] Each individual output is mono, or disabled if the host does not want it.
    for (int b = 1; b <= INDIV_OUTS; ++b)
    {
        auto set = layouts.getChannelSet(false, b);
        if (set != juce::AudioChannelSet::disabled() && set != juce::AudioChannelSet::mono())
            return false;
    }

    // 3. [SAMPLING] Sampler In may be disabled, mono or stereo - and nothing else. Disabled is the
    //    default and must stay valid, because that is the layout every existing session already has.
    auto inBus = layouts.getChannelSet(true, 0);
    if (inBus != juce::AudioChannelSet::disabled()
        && inBus != juce::AudioChannelSet::mono()
        && inBus != juce::AudioChannelSet::stereo())
        return false;

    return true;
}
#endif

void EnsoniqSD1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
      int numSamples = buffer.getNumSamples();
      if (numSamples <= 0) return; // Safety check
        
        // CRITICAL SAFETY GATE:
        // Logic Pro (AU) often calls processBlock before MAME's background thread is ready.
        // If we call mameMachine->time().as_double() before clocks are set, it triggers SIGFPE (divide by zero).
        if (!mameIsFullyBooted.load(std::memory_order_acquire)) {
            buffer.clear(); // Output silence until the engine is stable
            return;
        }
    
    // 1. Get the ACTUAL number of channels provided by the host in this specific callback
    int numChannels = buffer.getNumChannels();
    int totalNumInputChannels = getTotalNumInputChannels();

    // Capture the block position before the existing A/D ring is written.
    // This is also our zero-copy pre-record MON source; the processor's
    // validated REC GAIN stage has already been applied when read later.
    const uint64_t monitorInputStart = samplerInWritten.load(std::memory_order_relaxed);
    pushAudioToMame(buffer, totalNumInputChannels, numSamples);

    // 2. PROTECTION: Safely clear the output channels that physically exist in the buffer.
    // The VST3 standard (especially in strict DAWs like Studio One/Pro) might pass nullptrs
    // for unrouted outputs even if the reported channel count is higher. We MUST check for nullptr!
    //
    // [SAMPLING] This used to start at totalNumInputChannels, which was correct only while that was
    // ZERO. With an input bus enabled, channels 0..n-1 arrive holding the INPUT signal, and the output
    // loop below leaves its tail untouched in offline renders - so the input would have bled into Main
    // Out. Clearing from 0 fixes that, and when the input bus is off the loop is identical to before.
    for (int i = 0; i < numChannels; ++i) {
        auto* channelData = buffer.getWritePointer(i);
        if (channelData != nullptr) {
            juce::FloatVectorOperations::clear(channelData, numSamples);
        }
    }
    
    // --- State and Boot Checks ---
    if (sampleRateMismatch.load(std::memory_order_acquire)) return;
    if (!isMameRunningFlag()) return;
        
        // ========================================================
        // WARM BOOT PRE-ROLL (LOGIC PRO ONLY - Fixes silent 1st note)
        // ========================================================
        if (needsBootPreRoll.exchange(false, std::memory_order_acquire)) {
            if (wrapperType == juce::AudioProcessor::wrapperType_AudioUnit && mameMachine != nullptr) {
                
                // 1. Wait for MAME to consume the RAM injection and reset the virtual CPU
                int injTimeout = 1000;
                while (pendingRamInjection.load(std::memory_order_acquire) && injTimeout > 0) {
                    mameThrottleEvent.signal();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    injTimeout--;
                }

                // 2. Fast-forward MAME by 2.0 virtual seconds to let the SD-1 OS fully boot!
                double targetTime = mameMachine->time().as_double() + 2.0;
                int timeout = 3000;
                
                while (mameMachine->time().as_double() < targetTime && timeout > 0) {
                    uint64_t wPos = totalWritten.load(std::memory_order_relaxed);
                    if (wPos > totalRead.load(std::memory_order_relaxed)) {
                        totalRead.store(wPos, std::memory_order_relaxed); // Consume audio silently
                    }
                    mameThrottleEvent.signal();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    timeout--;
                }
                                
                // 3. Clean slate for the incoming MIDI notes
                totalRead.store(0, std::memory_order_release);
                totalWritten.store(mameBufferThreshold.load(std::memory_order_relaxed), std::memory_order_release);
                needAnchorSync.store(true, std::memory_order_release);
            }
        }
    
    uint64_t currentReadPos = totalRead.load(std::memory_order_acquire);
    int threshold = mameBufferThreshold.load(std::memory_order_relaxed);
    double sr = hostSampleRate.load(std::memory_order_relaxed);
    
    // Security boot check
    if (mameMachine == nullptr) {
        totalRead.store(currentReadPos + numSamples, std::memory_order_release);
        return;
    }
    
    // ==========================================================
    // AU SPECIFIC LOGIC PRO SYNC FIX (VST3 completely bypassed)
    // ==========================================================
        
    if (wrapperType == juce::AudioProcessor::wrapperType_AudioUnit) {
        
        bool currentOffline = isNonRealtime();
        bool offlineChanged = (currentOffline != lastOfflineState);
        lastOfflineState = currentOffline;
        bool freshPrepare = prepareWasCalled.exchange(false, std::memory_order_acq_rel);

        // Clear stale queue whenever prepareToPlay just ran OR on fresh start
        if (freshPrepare || currentReadPos == 0) {
            pendingAUMidi.clear();
        }

        // offlineChanged mid-session reset
        if (offlineChanged && !freshPrepare && currentReadPos > 0) {
            pendingAUMidi.clear();
            uint64_t newWritePos = currentReadPos + mameBufferThreshold.load(std::memory_order_relaxed);
            totalWritten.store(newWritePos, std::memory_order_release);
            maxOfflineBuffer.store(mameBufferThreshold.load(std::memory_order_relaxed), std::memory_order_relaxed);
            for (int j = 0; j < RING_BUFFER_SIZE; ++j) {
                ringBufferL[j] = 0.0f; ringBufferR[j] = 0.0f;
                for (int c = 0; c < INDIV_OUTS; ++c) ringBufferIndiv[c][j] = 0.0f;
            }
            midiReadPos.store(midiWritePos.load(std::memory_order_acquire), std::memory_order_release);
        }
    }
    
    // =====================================================================
    // DETECT TRANSPORT ORIGINAL
    // =====================================================================
    
    bool isPlaying = false;
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            isPlaying = pos->getIsPlaying();
        }
    }
    
    // --- DETECT LOGIC TRANSPORT & BOUNCE START ---
    bool justStartedPlaying = (isPlaying && !lastIsPlaying);
    lastIsPlaying = isPlaying;
    
    // Self-contained offline tracker to fix the scope issue
    bool isOffline = isNonRealtime();
    bool isBounceStart = (isOffline && !localLastOffline);
    localLastOffline = isOffline;

    // ==============================================================
    // MASCHINE PLAY-START FLUSH (wrapper-specific)
    // ==============================================================
    // When Maschine starts playback, the ring buffer contains stale audio
    // generated during idle time. This audio predates any MIDI events in
    // this block. If we read it, the first notes are silent/delayed by ~30ms.
    // Fix: at play-start, skip all stale audio so MIDI events and audio
    // are generated together from the same point in time.
    {
        if (isMaschineHost && justStartedPlaying) {
            maschineInFastRender = false;

            bool mameBooted = (mameMachine != nullptr && mameMachine->time().as_double() > 3.0);
            if (mameBooted) {
                uint64_t writePos = totalWritten.load(std::memory_order_acquire);
                if (writePos > currentReadPos) {
                    currentReadPos = writePos;
                    totalRead.store(currentReadPos, std::memory_order_release);
                }
                needAnchorSync.store(true, std::memory_order_release);
            }
        }

        if (isMaschineHost && !isPlaying)
            maschineInFastRender = false;
    }
    
    // =====================================================================
    // AU CLOCK DRIFT
    // =====================================================================
    if (wrapperType == juce::AudioProcessor::wrapperType_AudioUnit) {
        if (justStartedPlaying || isBounceStart) {
            needAnchorSync.store(true, std::memory_order_release);
        }
    }
                
                // ========================================================
                // WAIT FOR PERFECT ANCHOR
                // ========================================================
                
                if (needAnchorSync.load(std::memory_order_acquire)) {
                    mameThrottleEvent.signal(); // Wake up MAME!
                    
                    // We determine how long we are allowed to block the audio thread.
                    int timeoutMs = isNonRealtime() ? 2000 : 2;
                    int waitMs = 0;
                    
                    // Wait loop: Check if MAME has established the exact audio anchor yet
                    while (needAnchorSync.load(std::memory_order_acquire) && waitMs < timeoutMs) {
    #if defined(_WIN32)
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    #elif defined(__APPLE__)
                        if (pthread_main_np() == 0) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                        else {
                            break; // Safety breakout for macOS main thread rendering
                        }
    #else
                        if (! juce::MessageManager::getInstance()->isThisTheMessageThread()) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                        else {
                            break;
                        }
    #endif
                        waitMs++;
                    }

                    // EMERGENCY FALLBACK (REALTIME ONLY):
                    if (needAnchorSync.load(std::memory_order_acquire)) {
                        if (!isNonRealtime() && mameMachine != nullptr) {
#if JucePlugin_Build_AU
        uint64_t anchorSample = totalWritten.load(std::memory_order_acquire);
        double capturedMameTime = mameMachine->time().as_double();
        anchorDawSample.store(anchorSample, std::memory_order_relaxed);
        anchorMameTime.store(capturedMameTime, std::memory_order_relaxed);
#else
                            anchorMameTime.store(mameMachine->time().as_double(), std::memory_order_relaxed);
                            anchorDawSample.store(totalWritten.load(std::memory_order_acquire), std::memory_order_relaxed);
                    #endif
                            needAnchorSync.store(false, std::memory_order_release);
                        }
                    }
                }
        
        // FRESH ANCHOR
        double t_anchor = anchorMameTime.load(std::memory_order_relaxed);
        uint64_t s_anchor = anchorDawSample.load(std::memory_order_relaxed);
    
    // Trigger the Logic Chase filter on block 0, transport start, or bounce start
    bool isLogicChaseDump = (currentReadPos == 0) || justStartedPlaying || isBounceStart;
        
    // -------------------------------
    // AU MIDI dispatch
    // -------------------------------
    
    // Suppress all incoming DAW MIDI during Write Single Program
    // (our pushMidiByte calls bypass this — they go directly to the ring buffer)
    if (suppressMidiInput.load(std::memory_order_acquire))
        midiMessages.clear();

    if (wrapperType == juce::AudioProcessor::wrapperType_AudioUnit)
    {
        bool mameHasAudio = totalWritten.load(std::memory_order_acquire) > static_cast<uint64_t>(threshold);
        
        double currentMameTime = (mameMachine != nullptr) ? mameMachine->time().as_double() : 0.0;

        // Anchor the clock to NOW so we don't accidentally stagger into the past
        if (isLogicChaseDump) {
            lastAuMidiTime = currentMameTime;
        }

        if (needAnchorSync.load(std::memory_order_acquire) || !mameHasAudio)
        {
            if (pendingAUMidi.empty()) {
                captureReadPos = currentReadPos;
            }

            for (const auto metadata : midiMessages) {
                auto msg = metadata.getMessage();
                
                if (isLogicChaseDump) {
                    bool keep = false;
                    // AGGRESSIVE CHASE FILTER: Drops ALL CCs on block 0!
                    // Keeps the UART buffer 100% clear for the initial Note On.
                    if (msg.isNoteOn() || msg.isNoteOff() || msg.isPitchWheel() || msg.isAftertouch() || msg.isChannelPressure()) {
                        keep = true;
                    }
                    if (!keep) continue;
                }
                
                int absoluteOffset = metadata.samplePosition + static_cast<int>(currentReadPos - captureReadPos);
                pendingAUMidi.push_back({msg, absoluteOffset});
            }
            mameThrottleEvent.signal();
        }
        else
        {
            // 1. Flush pending
            if (!pendingAUMidi.empty()) {
                for (auto& pending : pendingAUMidi) {
                    auto msg = pending.first;
                    
                    uint64_t absoluteDawSample = captureReadPos + pending.second;
                    uint64_t targetSample = absoluteDawSample + threshold;
                    
                    double targetMameTime = t_anchor + static_cast<double>(
                        static_cast<int64_t>(targetSample) - static_cast<int64_t>(s_anchor)) / sr;

                    const uint8_t* rawData = msg.getRawData();
                    for (int i = 0; i < msg.getRawDataSize(); ++i) {
                        
                        // FIX: Prevent 0-cycle UART Overrun!
                        // If queued events calculate to the past, clamp them to the present so the virtual CPU can read them.
                        if (targetMameTime < currentMameTime) targetMameTime = currentMameTime;
                        
                        if (targetMameTime < lastAuMidiTime + 0.00032) {
                            targetMameTime = lastAuMidiTime + 0.00032;
                        }
                        pushMidiByte(rawData[i], targetMameTime);
                        lastAuMidiTime = targetMameTime;
                    }
                }
                pendingAUMidi.clear();
            }

            // 2. Process current block
            for (const auto metadata : midiMessages) {
                auto msg = metadata.getMessage();
                int eventOffset = metadata.samplePosition;
                
                if (isLogicChaseDump) {
                    bool keep = false;
                    if (msg.isNoteOn() || msg.isNoteOff() || msg.isPitchWheel() || msg.isAftertouch() || msg.isChannelPressure()) {
                        keep = true;
                    }
                    if (!keep) continue;
                }

                double targetMameTime;

                if (!isPlaying && !isOffline) {
                    targetMameTime = currentMameTime + static_cast<double>(eventOffset) / sr;
                } else {
                    uint64_t targetSample = currentReadPos + eventOffset + threshold;
                    targetMameTime = t_anchor + static_cast<double>(
                        static_cast<int64_t>(targetSample) - static_cast<int64_t>(s_anchor)) / sr;
                }

                const uint8_t* rawData = msg.getRawData();
                for (int i = 0; i < msg.getRawDataSize(); ++i) {
                    
                    // FIX: Prevent 0-cycle UART Overrun!
                    if (targetMameTime < currentMameTime) targetMameTime = currentMameTime;
                    
                    if (targetMameTime < lastAuMidiTime + 0.00032) {
                        targetMameTime = lastAuMidiTime + 0.00032;
                    }
                    pushMidiByte(rawData[i], targetMameTime);
                    lastAuMidiTime = targetMameTime;
                }
            }
        }
    }
        else
        {
            // ========================================================
            // VST3 STANDARD MIDI DISPATCH
            // ========================================================
            
            // Maschine play-start chase filter: drop CCs on transport start (same as AU isLogicChaseDump).
            bool maschineChaseFilter = isMaschineHost && justStartedPlaying;
            
            // Effective threshold for Maschine:
            //   block 0 (justStartedPlaying): effectiveThreshold=0
            //     → past-clamp restores threshold effect in real-time (MAME is ahead)
            //     → in WAV export after flush (MAME at currentReadPos) gives minimal offset
            //   block 1+ (maschineInFastRender confirmed by wait loop): effectiveThreshold=0
            // effectiveThreshold: 0 for Maschine render blocks, normal otherwise
            bool isMaschineRenderBlock = isMaschineHost && (justStartedPlaying || maschineInFastRender);
            uint64_t effectiveThreshold = isMaschineRenderBlock ? 0u : static_cast<uint64_t>(threshold);

            double currentMameTimeVST = (mameMachine != nullptr) ? mameMachine->time().as_double() : 0.0;

            for (const auto& metadata : midiMessages) {
                auto msg = metadata.getMessage();
                int eventOffset = metadata.samplePosition;

                if (maschineChaseFilter) {
                    if (!msg.isNoteOn() && !msg.isNoteOff() && !msg.isPitchWheel()
                        && !msg.isAftertouch() && !msg.isChannelPressure())
                        continue;
                }

                double targetMameTime;
                if (!isPlaying && !isOffline) {
                    targetMameTime = currentMameTimeVST + static_cast<double>(eventOffset) / sr;
                } else {
                    uint64_t targetSample = currentReadPos + static_cast<uint64_t>(eventOffset) + effectiveThreshold;
                    targetMameTime = t_anchor + static_cast<double>(
                        static_cast<int64_t>(targetSample) - static_cast<int64_t>(s_anchor)) / sr;
                }
                if (targetMameTime < currentMameTimeVST) targetMameTime = currentMameTimeVST;

                const uint8_t* rawData = msg.getRawData();
                for (int i = 0; i < msg.getRawDataSize(); ++i)
                    pushMidiByte(rawData[i], targetMameTime);
            }
        }
    
        // ========================================================
        // 1. TIMESTAMPED MIDI INJECTION
        // ========================================================
        
            // --- DELAYED INTERNAL MIDI PANIC ---
            int currentPanicDelay = panicDelaySamples.load(std::memory_order_acquire);
            if (currentPanicDelay > 0) {
                currentPanicDelay -= numSamples;
                
                if (currentPanicDelay <= 0) {
                    // Send CC 123 and 120 safely, RESPECTING the physical MIDI baud rate!
                    // 31250 bps = ~0.00032 seconds per byte. We space them out to prevent CPU interrupt flood.
                    double timePerByte = 0.00032;
                    int byteCount = 0;

                    for (uint8_t ch = 0; ch < 16; ++ch) {
                        pushMidiByte(0xB0 | ch, t_anchor + (byteCount++ * timePerByte));
                        pushMidiByte(123,       t_anchor + (byteCount++ * timePerByte));
                        pushMidiByte(0,         t_anchor + (byteCount++ * timePerByte));

                        pushMidiByte(0xB0 | ch, t_anchor + (byteCount++ * timePerByte));
                        pushMidiByte(120,       t_anchor + (byteCount++ * timePerByte));
                        pushMidiByte(0,         t_anchor + (byteCount++ * timePerByte));
                    }
                    panicDelaySamples.store(0, std::memory_order_release);
                } else {
                    panicDelaySamples.store(currentPanicDelay, std::memory_order_release);
                }
            }
                                                        
    // ========================================================
    // 2. AUDIO OUT & RING BUFFER CONSUMPTION
    // ========================================================
    
    int timeoutMs = 0;
    
    if (isOffline) {
            timeoutMs = 2000;
            // --- BOUNCE JITTER FIX ---
            maxOfflineBuffer.store(numSamples + mameBufferThreshold.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
    
    if (timeoutMs > 0) {
        int elapsedMs = 0;
        uint64_t targetWritePos = currentReadPos + numSamples;
        
        if (isOffline) {
            targetWritePos += mameBufferThreshold.load(std::memory_order_relaxed);
        }
        
        while (isMameRunningFlag()) {
            uint64_t writePos = totalWritten.load(std::memory_order_acquire);
            
            // Ultra-stable baseline wait loop
            if (writePos >= targetWritePos) break;
            if (elapsedMs >= timeoutMs) break;
            
            mameThrottleEvent.signal();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            elapsedMs++;
        }
    }
    
    // ==============================================================
    // MASCHINE WAV RENDER FIX (wrapper-specific, does NOT touch above)
    // ==============================================================
    // Maschine renders WAV in real-time mode (isNonRealtime()=false) but
    // calls processBlock at max CPU speed (~100x faster than real-time).
    // The MAME thread runs at real-time speed → ring buffer underruns → silence gaps.
    // Fix: wait for MAME to produce enough samples, same as the offline path.
    {
        if (isMaschineHost && !isOffline && isPlaying && isMameRunningFlag()) {
            bool mameBooted = (mameMachine != nullptr && mameMachine->time().as_double() > 3.0);

            if (mameBooted) {
                uint64_t targetWritePos = currentReadPos + static_cast<uint64_t>(numSamples);
                int elapsedMs = 0;

                while (isMameRunningFlag()) {
                    uint64_t writePos = totalWritten.load(std::memory_order_acquire);
                    if (writePos >= targetWritePos) break;
                    if (elapsedMs >= 2000) break;
                    mameThrottleEvent.signal();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    elapsedMs++;
                }

                // Confirm fast-render mode for next block's effectiveThreshold detection.
                if (elapsedMs > 0)
                    maschineInFastRender = true;
            }
        }
    }
    
        uint64_t currentWritePos = totalWritten.load(std::memory_order_acquire);
        int64_t available = static_cast<int64_t>(currentWritePos) - static_cast<int64_t>(currentReadPos);
        // Calculate how many samples we can push
        int samplesToProcess = 0;
        if (available > 0) {
            samplesToProcess = (available < numSamples) ? static_cast<int>(available) : static_cast<int>(numSamples);
        }
        
        // 3. PROTECTION: Safely retrieve write pointers based on the actual buffer dimensions!
        // If the DAW nullified the bus, getWritePointer will return nullptr.
        auto* outL    = (numChannels > 0) ? buffer.getWritePointer(0) : nullptr;
        auto* outR    = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;
        // [MULTIOUT 4] channels 2..9 are the eight individual outputs, one mono bus each.
        float *outIndiv[INDIV_OUTS] = { nullptr };
        for (int c = 0; c < INDIV_OUTS; ++c)
            outIndiv[c] = (numChannels > 2 + c) ? buffer.getWritePointer(2 + c) : nullptr;
        
        // Consume samples from the ring buffers
        for (int i = 0; i < samplesToProcess; ++i) {
            
            // --- OPTIMIZATION ---
            // Bitwise AND (&) wrap-around instead of modulo (%)
            uint64_t idx = currentReadPos & (RING_BUFFER_SIZE - 1);
            
            // 4. PROTECTION: Even Main L/R must be checked against nullptr to be 100% crash-proof
            if (outL != nullptr)    outL[i]    = ringBufferL[idx];
            if (outR != nullptr)    outR[i]    = ringBufferR[idx];
            for (int c = 0; c < INDIV_OUTS; ++c)
                if (outIndiv[c] != nullptr) outIndiv[c][i] = ringBufferIndiv[c][idx];
            
            currentReadPos++;
        }
        
        // Underrun protection: pad remaining required samples with zeroes
        if (!isNonRealtime() && samplesToProcess < numSamples) {
            for (int i = samplesToProcess; i < numSamples; ++i) {
                if (outL != nullptr)    outL[i]    = 0.0f;
                if (outR != nullptr)    outR[i]    = 0.0f;
                for (int c = 0; c < INDIV_OUTS; ++c)
                    if (outIndiv[c] != nullptr) outIndiv[c][i] = 0.0f;
            }
        }
        
        totalRead.store(currentReadPos, std::memory_order_release);
        mameThrottleEvent.signal(); // Wake MAME for the next round
        
        // ==============================================================
        // MIDI OUTPUT: Drain the SD-1 DUART TX → JUCE MidiBuffer
        // ==============================================================
        {
            int readPos = midiOutReadPos.load(std::memory_order_acquire);
            int writePos = midiOutWritePos.load(std::memory_order_acquire);
            
            while (readPos != writePos) {
                uint8_t byte = midiOutBuffer[readPos & (MIDI_OUT_BUFFER_SIZE - 1)];
                readPos = (readPos + 1) & (MIDI_OUT_BUFFER_SIZE - 1);
                
                // Real-time messages (F8-FF): always 1 byte, can appear anywhere
                if (byte >= 0xF8) {
                    midiMessages.addEvent(&byte, 1, 0);
                    continue;
                }
                
                // SysEx handling
                if (byte == 0xF0) {
                    midiOutMsg.clear();
                    midiOutMsg.push_back(byte);
                    midiOutInSysEx = true;
                    continue;
                }
                
                if (midiOutInSysEx) {
                    midiOutMsg.push_back(byte);
                    if (byte == 0xF7) {
                        // Complete SysEx message
                        midiMessages.addEvent(midiOutMsg.data(), (int)midiOutMsg.size(), 0);
                        midiOutMsg.clear();
                        midiOutInSysEx = false;
                    }
                    // Safety: cap SysEx at 64KB to prevent runaway accumulation
                    if (midiOutMsg.size() > 65536) {
                        midiOutMsg.clear();
                        midiOutInSysEx = false;
                    }
                    continue;
                }
                
                // Status byte (new message)
                if (byte & 0x80) {
                    midiOutMsg.clear();
                    midiOutMsg.push_back(byte);
                    midiOutRunningStatus = byte;
                    
                    // System common messages with no data bytes
                    if (byte == 0xF6 || byte == 0xF7) {
                        midiMessages.addEvent(midiOutMsg.data(), 1, 0);
                        midiOutMsg.clear();
                    }
                    continue;
                }
                
                // Data byte
                midiOutMsg.push_back(byte);
                
                // Determine expected message length from status byte
                int expectedLen = 0;
                uint8_t status = (midiOutRunningStatus & 0xF0);
                if (status == 0xC0 || status == 0xD0) expectedLen = 2;       // Program Change, Channel Pressure
                else if (status >= 0x80 && status <= 0xE0) expectedLen = 3;  // Note, CC, Pitch Bend, etc.
                else if (midiOutRunningStatus == 0xF1 || midiOutRunningStatus == 0xF3) expectedLen = 2;  // MTC, Song Select
                else if (midiOutRunningStatus == 0xF2) expectedLen = 3;      // Song Position
                
                if (expectedLen > 0 && (int)midiOutMsg.size() >= expectedLen) {
                    midiMessages.addEvent(midiOutMsg.data(), (int)midiOutMsg.size(), 0);
                    midiOutMsg.clear();
                }
            }
            
            midiOutReadPos.store(readPos, std::memory_order_release);
        }
        
        // --- ANTI-SMART-DISABLE Protection ---
        // Flipping sign sample-by-sample to generate inaudible Nyquist noise
        // NEW: Only inject noise during real-time playback
        if (!isNonRealtime()) {
            static float antiDisable = 1e-8f;
            for (int i = 0; i < numSamples; ++i) {
                antiDisable = -antiDisable; // Sample-by-sample flip!
                if (outL != nullptr) outL[i] += antiDisable;
                if (outR != nullptr) outR[i] += antiDisable;
            }
        }
    
                // P002: MON. Decode real firmware WADCS writes, not GUI keys.
                // Bit 5 as pre-record monitor inhibit is an evidence-based
                // candidate (A2 idle / 82 ARM), pending MON/Moff verification.
                // Disable direct pass-through during GO, leaving native
                // firmware/DSP monitor voices unchanged.
                const auto monitorMode = AkaiFirmwareMonitor::decode(
                    firmwareWadcsLatch.load(std::memory_order_acquire),
                    firmwareWadcsKnown.load(std::memory_order_acquire));
                const bool monitorInputValid = totalNumInputChannels > 0
                    && numSamples <= SAMPLER_IN_FRAMES;
                const float monitorTarget = (monitorInputValid
                    && monitorMode == AkaiFirmwareMonitor::Mode::Prerecord) ? 1.0f : 0.0f;
                if (monitorTarget > 0.0f || smoothedMonitorAmount > 0.0f) {
                    const float step = AkaiAnalogControls::smoothingCoefficient(static_cast<float>(sr));
                    for (int i = 0; i < numSamples; ++i) {
                        smoothedMonitorAmount = AkaiAnalogControls::smooth(smoothedMonitorAmount,
                                                                           monitorTarget, step);
                        const size_t slot = static_cast<size_t>((monitorInputStart + i)
                                & (SAMPLER_IN_FRAMES - 1)) * SAMPLER_IN_CHANNELS;
                        if (monitorInputValid && outL != nullptr)
                            outL[i] = AkaiFirmwareMonitor::mix(outL[i],
                                samplerInRing[slot + 0], smoothedMonitorAmount);
                        if (monitorInputValid && outR != nullptr)
                            outR[i] = AkaiFirmwareMonitor::mix(outR[i],
                                samplerInRing[slot + 1], smoothedMonitorAmount);
                    }
                }

                // Physical MAIN VOLUME: attenuate only stereo L/R after original
                // MAME DSP. Individual outs 1-8 remain bit-identical.
                const float mainTarget = mainVolumeParameter != nullptr
                    ? AkaiAnalogControls::normalized(mainVolumeParameter->load(std::memory_order_relaxed)) : 1.0f;
                const float mainStep = AkaiAnalogControls::smoothingCoefficient(static_cast<float>(sr));
                for (int i = 0; i < numSamples; ++i) {
                    smoothedMainVolume = AkaiAnalogControls::smooth(smoothedMainVolume, mainTarget, mainStep);
                    if (outL != nullptr) outL[i] *= smoothedMainVolume;
                    if (outR != nullptr) outR[i] *= smoothedMainVolume;
                }

                // ========================================================
                // AU BOUNCE "NO-READ-AHEAD" GATE (Logic Pro specifically isolated)
                // ========================================================
                if (wrapperType == juce::AudioProcessor::wrapperType_AudioUnit && isNonRealtime()) {
                    maxOfflineBuffer.store(mameBufferThreshold.load(std::memory_order_relaxed), std::memory_order_relaxed);
                }
    
    }

//==============================================================================

bool EnsoniqSD1AudioProcessor::hasEditor() const
{
    return true; 
}

juce::AudioProcessorEditor* EnsoniqSD1AudioProcessor::createEditor()
{
    return new EnsoniqSD1AudioProcessorEditor (*this);
}

// ==============================================================================
// LEGACY MAME STATE EXTRACTOR (v0.9.7 to v0.9.8 Bridge)
// ==============================================================================
bool EnsoniqSD1AudioProcessor::extractLegacyMameState(const juce::String& base64State,
                                                      juce::MemoryBlock& outOsram,
                                                      juce::MemoryBlock& outSeqram)
{
    juce::MemoryBlock compressedBlock;
    
    // The string arrives here as valid Base64 from the JUCE XML parser
    if (!compressedBlock.fromBase64Encoding(base64State) || compressedBlock.getSize() < 100) {
        return false;
    }

    const uint8_t* compData = static_cast<const uint8_t*>(compressedBlock.getData());
    size_t compSize = compressedBlock.getSize();

    // 1. Validating the MAME header on the ORIGINAL (unpacked) data!
    // The first 32 bytes of the MAME .sta file are NOT compressed!
    if (compSize < 32 || memcmp(compData, "MAMESAVE", 8) != 0) {
        return false;
    }

    // 2. Exactly locate the ZLIB header (it is usually located at offset 32 after the MAME header)
    int zlibOffset = 32;
    if (compData[zlibOffset] != 0x78) {
        // If for some reason it doesn't start at byte 32, we'll look for it:
        zlibOffset = -1;
        for (size_t i = 32; i < compSize - 1; ++i) {
            if (compData[i] == 0x78 && (compData[i+1] == 0x9C || compData[i+1] == 0xDA || compData[i+1] == 0x01)) {
                zlibOffset = static_cast<int>(i);
                break;
            }
        }
    }

    if (zlibOffset < 0) return false;

    // 3. Unpacking to raw memory
    juce::MemoryBlock uncompressedBlock;
    uLongf uncompressedSize = 10 * 1024 * 1024; // 10 MB safety limit for MAME states
    uncompressedBlock.setSize(uncompressedSize, true);
    
    int zResult = uncompress(static_cast<Bytef*>(uncompressedBlock.getData()),
                             &uncompressedSize,
                             compData + zlibOffset,
                             static_cast<uLong>(compSize - zlibOffset));
    if (zResult != Z_OK) return false;
    uncompressedBlock.setSize(uncompressedSize);

    const uint8_t* data = static_cast<const uint8_t*>(uncompressedBlock.getData());
    size_t size = uncompressedBlock.getSize();

    outOsram.setSize(0);
    outSeqram.setSize(0);

    // ====================================================================
    // 4. HARDCODED OFFSETS FOR MAME 0.286 (IN THE ZLIB PAYLOAD)
    // ====================================================================
    // Since zlib uncompress omitted the 32-byte header from the decompressed
    // buffer, we SUBTRACTED 32 FROM the offsets obtained from the MAME dumper.
    // 2104650 - 32 = 2104618
    // 2170186 - 32 = 2170154
    size_t osram_offset  = 2104618;
    size_t seqram_offset = 2170154;

    // 5. Direct copying with the correct offset
    if (osram_offset + 65536 <= size) {
        outOsram.append(data + osram_offset, 65536);
    }
    if (seqram_offset + 327680 <= size) {
        outSeqram.append(data + seqram_offset, 327680);
    }

    return (outOsram.getSize() == 65536 && outSeqram.getSize() == 327680);
}

//==============================================================================
// SAVE STATE
//==============================================================================
void EnsoniqSD1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // 1. Save VST parameters to XML
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    // --- 1. VERSION STAMPING ---
    xml->setAttribute("plugin_version", "1.0.0");
    
    // --- 2. SAVE UI DIMENSIONS ---
    xml->setAttribute("ui_width", savedWindowWidth);
    xml->setAttribute("ui_height", savedWindowHeight);

    // --- 3. SAVE MEDIA PATHS NATIVELY ---
    {
        std::lock_guard<std::mutex> lock(mediaMutex);
        xml->setAttribute("floppy_path", juce::String(pendingFloppyPath));
        xml->setAttribute("cart_path", juce::String(pendingCartPath));
        xml->setAttribute("is_floppy_loaded", isFloppyLoaded.load());
        xml->setAttribute("is_cart_loaded", isCartLoaded.load());
    }

    // P017: capture the full original-MAME machine state, including Akai
    // sample RAM, DSP and CPU registers, not merely old Ensoniq NVRAM.
    // MAME owner thread performs the operation. This host callback never
    // reads/writes MAME registers directly for the native snapshot.
    {
        std::lock_guard<std::mutex> hostLock(nativeStateHostMutex);
        juce::MemoryBlock machineBytes;
        bool haveMachine=false;
        // An in-flight project restore is more authoritative than whatever
        // old/empty machine state is currently running. A save while the MAME
        // worker is restoring must preserve the incoming exact original .sta.
        const auto unresolved= nativeRestore.unresolved();
        if(unresolved) {
            machineBytes.append(unresolved->data(),unresolved->size());
            haveMachine=true;
        } else if(isMameRunningFlag() && mameIsFullyBooted.load(std::memory_order_acquire)) {
            const auto seq=nativeSnapshotRequested.fetch_add(
                1,std::memory_order_acq_rel)+1;
            nativeSnapshotEvent.reset();
            constexpr int timeoutMs=6000;
            int waited=0;
            while(waited<timeoutMs &&
                  nativeSnapshotFinished.load(std::memory_order_acquire)!=seq) {
                (void)nativeSnapshotEvent.wait(200);
                waited+=200;
            }
            if(nativeSnapshotFinished.load(std::memory_order_acquire)==seq &&
               nativeSnapshotSucceeded.load(std::memory_order_acquire)) {
                const auto path=nativeSnapshotFile();
                if(path.existsAsFile() && path.getSize()>=32
                   && path.getSize()<=static_cast<juce::int64>(
                       akai::machine_state::maxSnapshot)) {
                    haveMachine=path.loadFileAsData(machineBytes);
                }
            }
        } else {
            // Preserve an incoming snapshot when DAW saves before firmware boot.
            const auto retained=nativeRestore.current();
            if(retained && !retained->empty()) {
                machineBytes.append(retained->data(),retained->size());
                haveMachine=true;
            }
        }
        std::vector<std::uint8_t> envelope;
        if(haveMachine && akai::machine_state::encode(
            std::span<const std::uint8_t>(
                static_cast<const std::uint8_t*>(machineBytes.getData()),
                machineBytes.getSize()),envelope)) {
            const juce::MemoryBlock encoded(envelope.data(),envelope.size());
            xml->setAttribute("akai_native_machine_v1",encoded.toBase64Encoding());
            xml->setAttribute("akai_native_complete",true);
            xml->setAttribute("akai_native_restore_failed",
                              nativeRestore.lastAttemptFailed());
        } else {
            // A missing snapshot must be VISIBLE to later QA. Old RAM
            // fragments are NOT a playable Akai project snapshot.
            xml->setAttribute("akai_native_complete",false);
        }
    }

    // P017: do not access MAME-owned raw shares from the DAW thread.
    // The complete native .sta envelope above replaces the old partial
    // OSRAM/SEQRAM snapshot for NEW songs. Existing legacy XML projects
    // remain readable by the backward-compatible load branch below.
    
    // --- 4.5 FLUSH ACTIVE UI STATE ---
        if (auto* editor = dynamic_cast<EnsoniqSD1AudioProcessorEditor*>(getActiveEditor())) {
            editor->flushFileManagerState();
        }
    
    // --- 5. SAVE FILE MANAGER STATE ---
    {
        auto& fms = fileManagerState;
        xml->setAttribute("fm_visible", fms.visible);
        xml->setAttribute("fm_category", fms.category);
        xml->setAttribute("fm_file", fms.openedFilePath);
        xml->setAttribute("fm_viewingBank", fms.viewingDiskBank);
        xml->setAttribute("fm_diskBankName", fms.openedDiskBankName);
        xml->setAttribute("fm_selectedRow", fms.selectedRow);
        xml->setAttribute("fm_bankRow", fms.bankSelectedRow);
        xml->setAttribute("fm_selectedName", fms.selectedName);
        xml->setAttribute("fm_bankSelectedName", fms.bankSelectedName);
        xml->setAttribute("fm_scroll", fms.scrollPosition);
        xml->setAttribute("fm_bankScroll", fms.bankScrollPosition);
        xml->setAttribute("fm_activeBookmark", fms.activeBookmark);
        xml->setAttribute("fm_viewBefore", fms.viewBeforeBrowser);
        xml->setAttribute("fm_width", fms.fmWindowWidth);
        xml->setAttribute("fm_height", fms.fmWindowHeight);
    }
    
    copyXmlToBinary (*xml, destData);
}

//==============================================================================
// LOAD STATE
//==============================================================================
void EnsoniqSD1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr) {
        // Transactionally validate the whole original-MAME state BEFORE
        // changing APVTS, file paths or any old RAM recall controls.
        // Malformed data cannot silently fall back to partial guest RAM.
        std::shared_ptr<const std::vector<std::uint8_t>> validatedNative;
        const bool hasNative=xmlState->hasAttribute("akai_native_machine_v1");
        if(hasNative) {
            const auto encoded=xmlState->getStringAttribute("akai_native_machine_v1");
            if(encoded.length()>static_cast<int>(akai::machine_state::maxSnapshot*2))
                return;
            juce::MemoryBlock decoded;
            if(!decoded.fromBase64Encoding(encoded))
                return;
            std::vector<std::uint8_t> raw;
            if(!akai::machine_state::decode(std::span<const std::uint8_t>(
                static_cast<const std::uint8_t*>(decoded.getData()),
                decoded.getSize()),raw))
                return;
            validatedNative=std::make_shared<const std::vector<std::uint8_t>>(
                std::move(raw));
        }
        {
            std::lock_guard<std::mutex> hostGuard(nativeStateHostMutex);
            nativeRestore.publish(std::move(validatedNative));
        }

        // 1. Restore VST Automation Parameters
        if (xmlState->hasTagName (apvts.state.getType())) {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        }

        // 2. Restore UI Dimensions
        savedWindowWidth = xmlState->getIntAttribute("ui_width", 0);
        savedWindowHeight = xmlState->getIntAttribute("ui_height", 0);

        // =================================================================
        // RAM EXTRACTION & MEDIA PATH RESTORATION
        // =================================================================
        juce::String savedVersion = xmlState->getStringAttribute("plugin_version", "0.9.7");
        
                // --- 3. APPLY MEDIA PATHS AND TRIGGER MOUNTING ---
                // Legacy 0.9.7 projects won't have these XML attributes. They default to empty strings.
                // For 0.9.8+, we restore the paths and strictly verify physical file existence on the disk.
                {
                    std::lock_guard<std::mutex> lock(mediaMutex);
                    juce::String savedFloppy = xmlState->getStringAttribute("floppy_path", "");
                    juce::String savedCart = xmlState->getStringAttribute("cart_path", "");
                    
                    bool floppyWasLoaded = xmlState->getBoolAttribute("is_floppy_loaded", false);
                    bool cartWasLoaded = xmlState->getBoolAttribute("is_cart_loaded", false);

                    // VERIFY FLOPPY EXISTENCE
                    if (floppyWasLoaded) {
                        if (juce::File(savedFloppy).existsAsFile()) {
                #ifdef _WIN32
                            pendingFloppyPath = savedFloppy.toUTF8().getAddress();
                #else
                            pendingFloppyPath = savedFloppy.toStdString();
                #endif
                            requestFloppyLoad.store(true, std::memory_order_release);
                            isFloppyLoaded.store(true, std::memory_order_release);
                            loadedFloppyName = juce::File(savedFloppy).getFileName();
                        } else {
                            pendingFloppyPath = "";
                            requestFloppyLoad.store(false, std::memory_order_release);
                            isFloppyLoaded.store(true, std::memory_order_release);
                            loadedFloppyName = "Missing file!";
                        }
                    } else {
                        pendingFloppyPath = "";
                        requestFloppyLoad.store(false, std::memory_order_release);
                        isFloppyLoaded.store(false, std::memory_order_release);
                        loadedFloppyName = "";
                    }

                    // VERIFY CARTRIDGE EXISTENCE
                    if (cartWasLoaded) {
                        if (juce::File(savedCart).existsAsFile()) {
                #ifdef _WIN32
                            pendingCartPath = savedCart.toUTF8().getAddress();
                #else
                            pendingCartPath = savedCart.toStdString();
                #endif
                            requestCartLoad.store(true, std::memory_order_release);
                            isCartLoaded.store(true, std::memory_order_release);
                            loadedCartName = juce::File(savedCart).getFileName();
                        } else {
                            pendingCartPath = "";
                            requestCartLoad.store(false, std::memory_order_release);
                            isCartLoaded.store(true, std::memory_order_release);
                            loadedCartName = "Missing file!";
                        }
                    } else {
                        pendingCartPath = "";
                        requestCartLoad.store(false, std::memory_order_release);
                        isCartLoaded.store(false, std::memory_order_release);
                        loadedCartName = "";
                    }
                }

                // --- 4. RAM INJECTION BRANCHING ---
                if (!hasNative && savedVersion == "0.9.7"
                    && xmlState->hasAttribute("mame_state")) {
                    
                    juce::String b64String = xmlState->getStringAttribute("mame_state");

                    // LEGACY LOAD (v0.9.7)
                    if (extractLegacyMameState(b64String, pendingOsram, pendingSeqRam)) {
                        pendingRamInjection.store(true, std::memory_order_release);
                        needsBootPreRoll.store(true, std::memory_order_release);
                        isWarmBoot.store(true, std::memory_order_release); // LOAD STATE FLAG
                    }
                }
                else if (!hasNative && xmlState->hasAttribute("ram_osram")) {
                    
                    // LOAD FROM NEW XML FORMAT
                    pendingOsram.fromBase64Encoding(xmlState->getStringAttribute("ram_osram"));
                    pendingSeqRam.fromBase64Encoding(xmlState->getStringAttribute("ram_seqram"));
                    pendingRamInjection.store(true, std::memory_order_release);
                    needsBootPreRoll.store(true, std::memory_order_release);
                    isWarmBoot.store(true, std::memory_order_release); // LOAD STATE FLAG
                }

                requestMameLoad.store(false, std::memory_order_release);
            }
        
            // --- RESTORE FILE MANAGER STATE ---
            {
                auto& fms = fileManagerState;
                fms.visible = xmlState->getBoolAttribute("fm_visible", false);
                fms.category = xmlState->getStringAttribute("fm_category", "");
                fms.openedFilePath = xmlState->getStringAttribute("fm_file", "");
                fms.viewingDiskBank = xmlState->getBoolAttribute("fm_viewingBank", false);
                fms.openedDiskBankName = xmlState->getStringAttribute("fm_diskBankName", "");
                fms.selectedRow = xmlState->getIntAttribute("fm_selectedRow", -1);
                fms.bankSelectedRow = xmlState->getIntAttribute("fm_bankRow", -1);
                fms.selectedName = xmlState->getStringAttribute("fm_selectedName", "");
                fms.bankSelectedName = xmlState->getStringAttribute("fm_bankSelectedName", "");
                fms.scrollPosition = xmlState->getIntAttribute("fm_scroll", 0);
                fms.bankScrollPosition = xmlState->getIntAttribute("fm_bankScroll", 0);
                fms.activeBookmark = xmlState->getStringAttribute("fm_activeBookmark", "");
                fms.viewBeforeBrowser = xmlState->getIntAttribute("fm_viewBefore", 0);
                fms.fmWindowWidth = xmlState->getIntAttribute("fm_width", 1200);
                fms.fmWindowHeight = xmlState->getIntAttribute("fm_height", 925);
                
                // If the song had an active bookmark, add it to bookmarks if not already there
                if (fms.activeBookmark.isNotEmpty()) {
                    juce::File bookmarkDir(fms.activeBookmark);
                    if (bookmarkDir.isDirectory() && !bookmarkFolders.contains(fms.activeBookmark)) {
                        if (bookmarkFolders.size() >= 5)
                            bookmarkFolders.remove(0);
                        bookmarkFolders.add(fms.activeBookmark);
                    }
                }
                
                // Validate saved paths/categories; fall back to defaults if missing
                if (fms.openedFilePath.isNotEmpty() && !juce::File(fms.openedFilePath).existsAsFile()) {
                    fms.openedFilePath = "";
                    fms.selectedName = "";       // file gone → no name-based restore
                    fms.viewingDiskBank = false;
                    fms.openedDiskBankName = "";
                }
                if (fms.category.startsWith("BOOKMARK:")) {
                    juce::String path = fms.category.substring(9);
                    if (!juce::File(path).isDirectory()) {
                        // Bookmarked folder gone → fall back to INT RAM
                        fms.category = "INT (RAM)";
                        fms.selectedRow = -1;
                        fms.selectedName = "";
                        fms.activeBookmark = "";
                    }
                }
            }
            stateJustLoaded.store(true, std::memory_order_release);
            requestFileManagerUIRefresh.store(true, std::memory_order_release);
    
            // Ensure COMPARE is properly evaluated even if the Editor GUI is closed during load
            if (mameMachine != nullptr) {
                    scheduledCompareResetTime.store(mameMachine->time().as_double() + 3.5, std::memory_order_release);
                } else {
                    scheduledCompareResetTime.store(3.5, std::memory_order_release);
                }
    
}

// ==============================================================================

void EnsoniqSD1AudioProcessor::injectMouseMove(int x, int y) {
    mouseX.store(x, std::memory_order_relaxed);
    mouseY.store(y, std::memory_order_relaxed);
}

void EnsoniqSD1AudioProcessor::injectMouseDown(int x, int y) {
    mouseX.store(x, std::memory_order_relaxed);
    mouseY.store(y, std::memory_order_relaxed);
    mouseButtons.fetch_or(1, std::memory_order_relaxed); // Set Left Click Bit
}

void EnsoniqSD1AudioProcessor::injectMouseUp(int x, int y) {
    mouseX.store(x, std::memory_order_relaxed);
    mouseY.store(y, std::memory_order_relaxed);
    mouseButtons.fetch_and(~1, std::memory_order_relaxed); // Clear Left Click Bit
}

//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EnsoniqSD1AudioProcessor();
}

// ==============================================================================
// ENSONIQ MAME DRIVER LIST REGISTRATION
// ==============================================================================
extern const game_driver driver____empty;
extern const game_driver driver_s3000xl;

const game_driver * const driver_list::s_drivers_sorted[] =
{
    &driver____empty,
    &driver_s3000xl,
};

const std::size_t driver_list::s_driver_count =
    sizeof(driver_list::s_drivers_sorted) / sizeof(driver_list::s_drivers_sorted[0]);

// ==============================================================================
// Selfcheck
// ==============================================================================

bool EnsoniqSD1AudioProcessor::runSelfCheck()
{
    juce::StringArray errors;
    
        // --- UNSUPPORTED AU HOST WARNING CHECK ---
        // Whitelist DAWs that correctly support the Logic AU synchronization path.
        // For other hosts (like FL Studio), we flag to advise the VST3 version.
        if (wrapperType == juce::AudioProcessor::wrapperType_AudioUnit) {
            juce::PluginHostType host;
            juce::String hostPath = host.getHostPath().toLowerCase();
            if (!host.isLogic() &&
                !host.isGarageBand() &&
                !host.isAbletonLive() &&
                !host.isReaper() &&
                !host.isStudioOne() &&
                !hostPath.contains("fender") &&
                !hostPath.contains("studio pro")) {
                
                isUnsupportedAUHost.store(true, std::memory_order_release);
            }
        }
    
    // --- 1. DOCUMENTS FOLDER WRITE ACCESS (For settings.xml and Master State) ---
        juce::File docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::File ensoniqDir = docsDir.getChildFile("S3000XL");
        
        if (!ensoniqDir.exists()) {
            auto result = ensoniqDir.createDirectory();
            if (!result.wasOk()) {
                errors.add("- Cannot create directory: " + ensoniqDir.getFullPathName());
            }
        }
        
        if (ensoniqDir.exists()) {
            // Test parent folder
            juce::File testFile = ensoniqDir.getChildFile(".write_test");
            if (testFile.replaceWithText("test")) {
                testFile.deleteFile();
            } else {
                errors.add("- No write permission to: " + ensoniqDir.getFullPathName());
            }

            // --- 1.5 GLOBAL STATE FOLDER CHECK (New for Master Sync logic) ---
            juce::File globalStateDir = ensoniqDir.getChildFile("GlobalState");
            if (!globalStateDir.exists()) {
                auto result = globalStateDir.createDirectory();
                if (!result.wasOk()) {
                    errors.add("- Cannot create GlobalState directory in Documents!");
                }
            } else {
                // Test write access to existing GlobalState
                juce::File gsTestFile = globalStateDir.getChildFile(".gs_write_test");
                if (gsTestFile.replaceWithText("test")) {
                    gsTestFile.deleteFile();
                } else {
                    errors.add("- No write permission to GlobalState folder (used for Master NVRAM)!");
                }
            }
        }

    // --- 2. TEMP FOLDER WRITE ACCESS (Critical for MAME plugins ---
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File tempTestFile = tempDir.getChildFile(".vst_temp_test");
    if (tempTestFile.replaceWithText("test")) {
        tempTestFile.deleteFile();
    } else {
        errors.add("- No write permission to OS Temp directory!");
    }

    // --- 3. PLUGINS FOLDER EXISTENCE & EXTRACTION (Cross-platform Sandbox & Antivirus check) ---
    #ifndef _WIN32
        // macOS / Linux AU/VST3 Sandbox check
        juce::File exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        juce::File contentsDir = exeFile.getParentDirectory().getParentDirectory();
        juce::File pluginsDir = contentsDir.getChildFile("Resources").getChildFile("plugins");
        
        if (!pluginsDir.isDirectory() && wrapperType == juce::AudioProcessor::wrapperType_AudioUnit) {
            juce::File candidate = exeFile;
            for (int i = 0; i < 6 && candidate.exists(); ++i) {
                if (candidate.getFileExtension() == ".component") {
                    pluginsDir = candidate.getChildFile("Contents/Resources/plugins");
                    break;
                }
                candidate = candidate.getParentDirectory();
            }
        }

        if (!pluginsDir.isDirectory()) {
            errors.add("- Missing MAME plugins folder: Resources/plugins");
        }
    #else
        // Windows: Extract plugins to Temp dir safely using a lock to prevent concurrent extraction
            juce::File tempMameDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("S3000XL_MAME_Data");
            juce::File requiredPluginFile = tempMameDir.getChildFile("plugins").getChildFile("layout").getChildFile("init.lua");

            if (!requiredPluginFile.existsAsFile())
            {
                juce::InterProcessLock extractLock("S3000XL_Plugin_Extract_Lock");
                if (extractLock.enter(5000)) // Wait up to 5s for another instance to finish unzipping
                {
                    // Double check after lock acquisition
                    if (!requiredPluginFile.existsAsFile())
                    {
                        tempMameDir.createDirectory();
                        juce::MemoryInputStream zipStream(BinaryData::mame_plugins_zip, BinaryData::mame_plugins_zipSize, false);
                        juce::ZipFile zip(zipStream);
                        auto result = zip.uncompressTo(tempMameDir);
                        if (!result.wasOk()) {
                            errors.add("- Failed to extract MAME plugins. Blocked by Antivirus or permissions?");
                        }
                    }
                    extractLock.exit();
                }
            }
        
        // Double-check that it actually exists after potential extraction (Catches aggressive real-time Antivirus deletion)
        if (!requiredPluginFile.existsAsFile()) {
            errors.add("- MAME layout plugins are missing from the Windows Temp directory.");
        }
    #endif

    // --- EVALUATE RESULTS ---
    if (errors.size() > 0) {
        isSelfCheckFailed.store(true, std::memory_order_release);
        selfCheckErrorMsg = errors.joinIntoString("\n");
        return false;
    }
    
    isSelfCheckFailed.store(false, std::memory_order_release);
    return true;
}

// ==============================================================================
// Check ROMS
// ==============================================================================
#ifdef AKAI_EMBED_PRIVATE_ROMS
bool EnsoniqSD1AudioProcessor::prepareEmbeddedRoms()
{
    // Firmware bytes are compiled into the private VST3 from the user's
    // SHA256-verified ZIP.  MAME still uses its ORIGINAL ROM loader.
    // Export only to this instance's private, transient sandbox and remove
    // the entire sandbox in shutdownMame() after the emulation thread joins.
    const auto romDir=juce::File(instanceTempDir).getChildFile("ROMs")
        .getChildFile("s3000xl");
    if (!romDir.isDirectory() && !romDir.createDirectory().wasOk())
        return false;

    const auto install=[&](const char* name,const unsigned char* bytes,
                           std::size_t length) {
        const auto file=romDir.getChildFile(name);
        juce::MemoryBlock existing;
        if(file.existsAsFile() && file.loadFileAsData(existing)
            && existing.getSize()==length
            && std::memcmp(existing.getData(),bytes,length)==0)
            return true;
        if(!file.replaceWithData(bytes,length))
            return false;
        existing.reset();
        return file.loadFileAsData(existing) && existing.getSize()==length
            && std::memcmp(existing.getData(),bytes,length)==0;
    };
    return install("s3000xl_v2_0.bin",akai::private_rom::firmware,
                   akai::private_rom::firmware_size)
        && install("hd61830.bin",akai::private_rom::display,
                   akai::private_rom::display_size);
}
#endif



void EnsoniqSD1AudioProcessor::checkRomAndBootMame()
{
#ifdef AKAI_EMBED_PRIVATE_ROMS
    // Private build: MAME ROMs come solely from this plugin binary.
    // No Documents ROM folder, third-party install or user file selection.
    if (!prepareEmbeddedRoms()) {
        isRomMissing.store(true,std::memory_order_release);
        isRomInvalid.store(true,std::memory_order_release);
        isMameRunning.store(false,std::memory_order_release);
        missingFilesList.clear();
        missingFilesList = "Private ROM extraction or verification failed.";
        return;
    }
#elif defined(AKAI_RUNTIME_ROM_OVERLAY)
    // Binary overlay mode: local packer appends verified owner ROMs to THIS
    // Windows DLL. No separate ROM files are needed at deployment.
    // This ROM-free CI build deliberately fails closed until privately packed.
    if (!akai::private_overlay::stageForOriginalMame(juce::File(instanceTempDir))) {
        isRomMissing.store(true,std::memory_order_release);
        isRomInvalid.store(true,std::memory_order_release);
        isMameRunning.store(false,std::memory_order_release);
        missingFilesList.clear();
        missingFilesList = "Private original Akai ROMs are not embedded.";
        return;
    }
#else
    // --- 0. LEGACY FALLBACK (for ROM-free public diagnostic build) ---
    // Keep the original manual ROM selection behavior, but it is NOT the
    // final private deployment model.
    // Check if the old 'sd132.zip' exists in the base folder and migrate it
        juce::File baseDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("S3000XL");
        juce::File legacyZip = baseDir.getChildFile("sd132.zip");
        if (legacyZip.existsAsFile()) {
            extractRomsFromZip(legacyZip);
        }
    
    // 1. Silent Auto-Fix: Check if the specific target files are in the ROMs folder
        juce::File romsDir = baseDir.getChildFile("ROMs");
        if (romsDir.exists() && romsDir.isDirectory()) {
            
            // Specifically look for our target zip files (instant check)
            juce::File zip1 = romsDir.getChildFile("sd132.zip");
            if (zip1.existsAsFile()) extractRomsFromZip(zip1);
            
            juce::File zip2 = romsDir.getChildFile("esq2x40_vfx.zip");
            if (zip2.existsAsFile()) extractRomsFromZip(zip2);

            // Auto-copy any loose .bin files placed directly in the ROMs folder
            copyRomsFromFolder(romsDir);
        }
#endif

    // 2. Verify the two expected s3000xl files (from temp in PRIVATE)
    if (!verifyRomFiles()) {
        isRomMissing.store(true, std::memory_order_release);
        isRomInvalid.store(true, std::memory_order_release);
        isMameRunning = false;
    } else {
        isRomMissing.store(false, std::memory_order_release);
        isRomInvalid.store(false, std::memory_order_release);
        isMameRunning = true;
        
        // Safety net against std::terminate if thread wasn't joined
        if (mameThread.joinable()) {
            mameThread.join();
        }
        mameThread = std::thread(&EnsoniqSD1AudioProcessor::runMameEngine, this);
    }
}

// ==============================================================================
// ROM VALIDATION & EXTRACTION
// ==============================================================================
bool EnsoniqSD1AudioProcessor::verifyRomFiles()
{
    missingFilesList.clear();
#if defined(AKAI_EMBED_PRIVATE_ROMS) || defined(AKAI_RUNTIME_ROM_OVERLAY)
    juce::File sd132Dir = juce::File(instanceTempDir).getChildFile("ROMs").getChildFile("s3000xl");
#else
    juce::File sd132Dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                             .getChildFile("S3000XL").getChildFile("ROMs").getChildFile("s3000xl");
#endif

    juce::StringArray expectedFiles = {
        "s3000xl_v2_0.bin", "hd61830.bin"
    };

    bool allFound = true;
    for (const auto& expected : expectedFiles) {
        juce::File romFile = sd132Dir.getChildFile(expected);
        if (!romFile.existsAsFile()) {
            allFound = false;
            missingFilesList << "- " << expected << "\n";
        }
    }
    return allFound;
}

bool EnsoniqSD1AudioProcessor::extractRomsFromZip(const juce::File& zipFile)
{
    juce::File sd132Dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                             .getChildFile("S3000XL").getChildFile("ROMs").getChildFile("s3000xl");
    sd132Dir.createDirectory();

    juce::ZipFile zip(zipFile);
    bool extractedSomething = false;
    
    juce::StringArray expectedFiles = {
        "s3000xl_v2_0.bin", "hd61830.bin"
    };

    for (int i = 0; i < zip.getNumEntries(); ++i) {
        auto* entry = zip.getEntry(i);
        if (entry != nullptr) {
            juce::String entryName = entry->filename.replaceCharacter('\\', '/');
            juce::String baseName = entryName.substring(entryName.lastIndexOfChar('/') + 1).toLowerCase();

            if (expectedFiles.contains(baseName)) {
                std::unique_ptr<juce::InputStream> inStream(zip.createStreamForEntry(i));
                if (inStream != nullptr) {
                    juce::File destFile = sd132Dir.getChildFile(baseName);
                    destFile.deleteFile(); // Overwrite if exists
                    std::unique_ptr<juce::OutputStream> outStream(destFile.createOutputStream());
                    if (outStream != nullptr) {
                        outStream->writeFromInputStream(*inStream, -1);
                        extractedSomething = true;
                    }
                }
            }
        }
    }
    return extractedSomething;
}

bool EnsoniqSD1AudioProcessor::copyRomsFromFolder(const juce::File& sourceDir)
{
    juce::File sd132Dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                             .getChildFile("S3000XL").getChildFile("ROMs").getChildFile("s3000xl");
    sd132Dir.createDirectory();

    bool copiedSomething = false;
    juce::StringArray expectedFiles = {
        "s3000xl_v2_0.bin", "hd61830.bin"
    };

    for (const auto& expected : expectedFiles) {
        juce::File srcFile = sourceDir.getChildFile(expected);
        if (srcFile.existsAsFile()) {
            juce::File destFile = sd132Dir.getChildFile(expected);
            // Prevent copying onto itself if the source IS the destination
            if (srcFile != destFile) {
                destFile.deleteFile(); // Overwrite if exists
                if (srcFile.copyFileTo(destFile)) {
                    copiedSomething = true;
                }
            }
        }
    }
    return copiedSomething;
}

// ==============================================================================
// HARDWARE STATE DECODING API & ROM PARSER
// ==============================================================================

void EnsoniqSD1AudioProcessor::buildVfdDictionary()
{
    segmentToAscii.clear();
    
    // The SD-1 uses ASCII characters from 32 (Space) to 95 ('_').
    // If a segment mask doesn't match, we default to space.
    segmentToAscii[0x0000] = ' ';

    juce::File fontRom = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("S3000XL").getChildFile("ROMs")
        .getChildFile("s3000xl").getChildFile("esqvfd_font_vfx.bin");

    // The esqvfd_font_vfx.bin ROM is exactly 192 bytes (96 characters x 16-bit words)
    if (fontRom.existsAsFile() && fontRom.getSize() == 192) {
        juce::MemoryBlock romData;
        fontRom.loadFileAsData(romData);
        const uint8_t* data = static_cast<const uint8_t*>(romData.getData());
        
        for (int i = 0; i < 64; ++i) {
            uint16_t segmentMask = (data[i * 2] << 8) | data[(i * 2) + 1];
            char asciiChar = static_cast<char>(32 + i); // 32 is ' ' in ASCII

            if (segmentMask != 0x0000 && segmentToAscii.find(segmentMask) == segmentToAscii.end()) {
                segmentToAscii[segmentMask] = asciiChar;
            }
        }
    }
}

bool EnsoniqSD1AudioProcessor::isHardwareLedOn(int ledBitIndex)
{
    uint32_t mask = ledStateMask.load(std::memory_order_relaxed);
    return (mask & (1U << ledBitIndex)) != 0;
}

juce::String EnsoniqSD1AudioProcessor::getHardwareVfdText()
{
    juce::String vfdText = "";
    
    for (int i = 0; i < VFD_SIZE; ++i) {
        uint16_t rawMask = vfdSegments[i].load(std::memory_order_relaxed);
        
        // MAME sets the highest bit (0x8000) for the underline attribute.
        // We strip it here to match the pure character shape from our dictionary.
        uint16_t cleanSegment = rawMask & 0x7FFF;
        
        auto it = segmentToAscii.find(cleanSegment);
        if (it != segmentToAscii.end()) {
            vfdText += it->second;
        } else {
            vfdText += " "; // Fallback for unrecognized symbols to keep UI clean
        }
    }
    return vfdText;
}

// ==============================================================================
// MAME OUTPUT NOTIFIER (VFD & LED CATCHER)
// ==============================================================================
void EnsoniqSD1AudioProcessor::mameOutputNotifier(const char *outname, s32 value, void *param)
{
    auto* processor = static_cast<EnsoniqSD1AudioProcessor*>(param);
    if (processor == nullptr || outname == nullptr) return;

    // WADCS hardware latch forwarded by the patched S3000XL MAME driver.
    if (strcmp(outname, "s3000xl_wadcs") == 0) {
        const auto next=static_cast<std::uint8_t>(value & 0xff);
        const auto old=processor->firmwareWadcsLatch.exchange(next,std::memory_order_acq_rel);
        processor->firmwareWadcsKnown.store(true,std::memory_order_release);
        if (((old ^ next) & 0x08)!=0) {
            if ((next & 0x08)!=0)
                processor->wavGoSerial.fetch_add(1,std::memory_order_release);
            else
                processor->wavStopSerial.fetch_add(1,std::memory_order_release);
        }
    }
    else if (strcmp(outname, "lights") == 0) {
        processor->ledStateMask.store(static_cast<uint32_t>(value), std::memory_order_relaxed);
    }
    // 2. Catch the VFD 14-segment characters (vfd0 through vfd79)
    else if (strncmp(outname, "vfd", 3) == 0) {
        int idx = atoi(outname + 3);
        if (idx >= 0 && idx < VFD_SIZE) {
            processor->vfdSegments[idx].store(static_cast<uint16_t>(value), std::memory_order_relaxed);
        }
    }
}

// ==============================================================================
// RUN MAME
// ==============================================================================

void EnsoniqSD1AudioProcessor::runMameEngine()
{
    // This state belongs only to the newly started MAME worker.
    instanceAudio.reset();
#ifdef _WIN32
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsA("Pro Audio", &taskIndex);
    if (hTask != NULL) {
        AvSetMmThreadPriority(hTask, AVRT_PRIORITY_CRITICAL);
    }
    else {
        // Fallback if no MMCSS
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }
#endif

    // --- 1. Prevent MAME from hijacking the host OS audio/video drivers ---
#ifdef _WIN32
    _putenv("SDL_VIDEODRIVER=dummy");
    _putenv("SDL_AUDIODRIVER=dummy");
#else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    setenv("SDL_MAC_BACKGROUND_APP", "1", 1);
    setenv("SDL_EVENT_HANDLING", "0", 1);
#endif
    
    std::vector<std::string> args;
    args.push_back("mame");
    args.push_back("s3000xl");
    
    // --- 2. ROM PATH CONFIGURATION ---
#if defined(AKAI_EMBED_PRIVATE_ROMS) || defined(AKAI_RUNTIME_ROM_OVERLAY)
    // Only our short-lived per-instance extraction folder is visible to MAME.
    juce::File romsDir=juce::File(instanceTempDir).getChildFile("ROMs");
#else
    juce::File romsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                     .getChildFile("S3000XL").getChildFile("ROMs");
#endif
    romsDir.createDirectory();

        #ifdef _WIN32
            // Ensure strict UTF-8 string conversion on Windows to prevent MAME boot failures
            // if the directory contains non-ASCII characters.
            std::string safePath = romsDir.getFullPathName().toUTF8().getAddress();
        #else
            // macOS/Linux natively handle paths well
            std::string safePath = romsDir.getFullPathName().toStdString();
        #endif

            // Set the base ROM path for MAME
            args.push_back("-rompath");
            args.push_back(safePath);

        // --- S3000XL SCSI media ---
        // CD-ROM (scsi:4): plain "cdrom" and "cdrom_2x" share the SAME identity (Sony CDU-76S)
        // and differ ONLY in SCSI data timing. The S3000XL firmware needs "cdrom_2x"'s realistic
        // 2x pacing to read a disc CLEANLY: plain "cdrom" transfers data too fast and the sample
        // bytes get corrupted -> crackly loads (verified in-DAW: HDD & cdrom_2x load clean, plain
        // cdrom crackles). So use cdrom_2x WHEN a disc is loaded (clean read + firmware detection),
        // and plain "cdrom" when empty (fast, so an idle drive never stalls the firmware/live MIDI).
        if (isCdLoaded.load() && !pendingCdPath.empty()) {
            args.push_back("-scsi:4"); args.push_back("cdrom_2x");
            args.push_back("-cdrm");   args.push_back(pendingCdPath);
        } else {
            args.push_back("-scsi:4"); args.push_back("cdrom");
        }
        {
            std::lock_guard<std::mutex> lock(mediaMutex);
            // Hard disk (scsi:5) is a FIXED drive: only attach it when an image exists, and mount
            // the image on the COMMAND LINE so it is present BEFORE device_reset. Mounting a
            // harddisk at runtime via image.load crashes nscsi_harddisk_device::device_reset.
            if (isHddLoaded.load() && !pendingHddPath.empty()) {
                args.push_back("-scsi:5"); args.push_back("harddisk");
                args.push_back("-hard");  args.push_back(pendingHddPath);
            }
            // A saved CD image is inserted hot once the machine is up.
            // CD is boot-mounted via -cdrm above; no runtime hot-swap needed at startup.
        }
    
        // --- 2.5 INITIAL MEDIA MOUNTING (COLD BOOT) ---
        // At DAW project load, MAME is booted from scratch. We must pass the media via
        // command line so the SD-1 OS detects them correctly during hardware initialization!
        {
            std::lock_guard<std::mutex> lock(mediaMutex);
            if (isFloppyLoaded.load() && !pendingFloppyPath.empty()) {
                args.push_back("-flop");
                args.push_back(pendingFloppyPath);
                
                // Clear the dynamic load flag so VstOsdInterface doesn't try to double-load it
                requestFloppyLoad.store(false, std::memory_order_release);
            }
            if (isCartLoaded.load() && !pendingCartPath.empty()) {
                args.push_back("-cart");
                args.push_back(pendingCartPath);
                
                // Clear the dynamic load flag so VstOsdInterface doesn't try to double-load it
                requestCartLoad.store(false, std::memory_order_release);
            }
        }
    
    // --- 3. PLUGINS PATH CONFIGURATION ---
    #ifdef _WIN32
        // Extraction and verification is now securely handled in runSelfCheck() beforehand
        juce::File tempMameDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("S3000XL_MAME_Data");
        juce::String finalPluginsPath = tempMameDir.getChildFile("plugins").getFullPathName();
        
        args.push_back("-pluginspath");
        args.push_back(finalPluginsPath.toUTF8().getAddress());
    #else
        // Original macOS solution
        juce::File exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        juce::File pluginsDir = exeFile.getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("plugins");
        
        // AU Sandbox fallback routine
        if (!pluginsDir.isDirectory() && wrapperType == juce::AudioProcessor::wrapperType_AudioUnit) {
            juce::File candidate = exeFile;
            for (int i = 0; i < 6 && candidate.exists(); ++i) {
                if (candidate.getFileExtension() == ".component") {
                    pluginsDir = candidate.getChildFile("Contents/Resources/plugins");
                    break;
                }
                candidate = candidate.getParentDirectory();
            }
        }
        
        args.push_back("-pluginspath");
        args.push_back(pluginsDir.getFullPathName().toStdString());
    #endif

    args.push_back("-plugin");
    args.push_back("layout");

    // Use software rendering and our custom OSD sound module
    args.push_back("-video");
    args.push_back("soft");
    args.push_back("-sound");
    args.push_back("osd");
    args.push_back("-midiin");
    args.push_back("VST MIDI");
    
    // Disable MAME's internal pacing (we control this strictly via audio throttle)
    args.push_back("-nothrottle");
    args.push_back("-nosleep");
    args.push_back("-nowaitvsync");
    args.push_back("-nowindow");
    args.push_back("-nobackground_input");

    args.push_back("-noreadconfig");
    args.push_back("-skip_gameinfo");
    args.push_back("-samplerate");
    args.push_back(std::to_string(static_cast<int>(hostSampleRate.load())));
    
    // Disable physical inputs to prevent MAME from stealing focus
    args.push_back("-keyboardprovider");
    args.push_back("none");
    args.push_back("-mouseprovider");
    args.push_back("none");
    args.push_back("-joystickprovider");
    args.push_back("none");
        
    // --- 4. SANDBOX & MASTER STATE SYNCHRONIZATION ---
        juce::File instDir(instanceTempDir);
        instDir.createDirectory();

        juce::File masterDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                .getChildFile("S3000XL").getChildFile("GlobalState");
        masterDir.createDirectory();

        // Safely copy NVRAM from Master to Instance Sandbox
        juce::File masterNvram = masterDir.getChildFile("nvram").getChildFile("s3000xl");
        juce::File instNvram = instDir.getChildFile("nvram").getChildFile("s3000xl");
        
        if (masterNvram.exists() && masterNvram.isDirectory()) {
            instNvram.createDirectory();
            juce::File srcOsram = masterNvram.getChildFile("osram");
            juce::File srcSeqram = masterNvram.getChildFile("seqram");
            
            if (srcOsram.existsAsFile()) srcOsram.copyFileTo(instNvram.getChildFile("osram"));
            if (srcSeqram.existsAsFile()) srcSeqram.copyFileTo(instNvram.getChildFile("seqram"));
        }

        // Assign MAME isolated paths for this instance
        args.push_back("-nvram_directory");
    #ifdef _WIN32
        args.push_back(instDir.getChildFile("nvram").getFullPathName().toUTF8().getAddress());
        args.push_back("-cfg_directory");
        args.push_back(instDir.getChildFile("cfg").getFullPathName().toUTF8().getAddress());
        args.push_back("-state_directory");
        args.push_back(instDir.getChildFile("state").getFullPathName().toUTF8().getAddress());
    #else
        args.push_back(instDir.getChildFile("nvram").getFullPathName().toStdString());
        args.push_back("-cfg_directory");
        args.push_back(instDir.getChildFile("cfg").getFullPathName().toStdString());
        args.push_back("-state_directory");
        args.push_back(instDir.getChildFile("state").getFullPathName().toStdString());
    #endif
           
    // Boot the headless CLI Frontend
    auto* mameOpts = new osd_options();
    auto* headlessOsd = new VstOsdInterface(this, *mameOpts);
    auto* frontend = new cli_frontend(*mameOpts, *headlessOsd);
    
    int mameResult = frontend->execute(args);
    
    delete frontend;
    delete headlessOsd;
    delete mameOpts;

    // --- Graceful boot-failure handling ---
    // If MAME could not start (commonly: a SCSI image is exclusively locked by another app,
    // read-only, missing, or corrupt), never leave the audio thread waiting for a dead engine
    // (that freezes the DAW), and never crash-loop on a persisted bad media path.
    if (mameResult != 0) {
        isMameRunning = false;
        mameIsFullyBooted.store(false, std::memory_order_release);
        bool hadMedia = isHddLoaded.load() || isCdLoaded.load() || isFloppyLoaded.load();
        if (hadMedia) {
            {
                std::lock_guard<std::mutex> lock(mediaMutex);
                pendingHddPath.clear(); pendingCdPath.clear(); pendingFloppyPath.clear();
                loadedHddName = ""; loadedCdName = ""; loadedFloppyName = "";
            }
            isHddLoaded.store(false, std::memory_order_release);
            isCdLoaded.store(false, std::memory_order_release);
            isFloppyLoaded.store(false, std::memory_order_release);
            requestGlobalSave.store(true, std::memory_order_release); // persist cleared paths -> no crash-loop
        }
    }
}
