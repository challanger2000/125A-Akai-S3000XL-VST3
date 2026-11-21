#!/usr/bin/env python3
"""Inject a WAV-file converter-output test fixture into STANDALONE Linux MAME.

Runs after applying the production MAME patch, before compilation. This code
never changes the production MAME patch or the Windows VST3; it creates a
diagnostic executable requiring owner-private original ROMs only at LOCAL run.
WAV frames travel only through the Akai ADC FIFO -> DMA -> original firmware
sample allocator. This is NOT a test of VST3 microphone/input callback.
"""
import argparse
from pathlib import Path

def one(text,old,new):
    n=text.count(old)
    if n!=1: raise RuntimeError(f'expected exactly one upstream anchor, found {n}: {old[:100]}')
    return text.replace(old,new,1)

def patch(path):
    s=path.read_text(encoding='utf-8')
    s=one(s, '#include "sound/adc.h"', '''#include "sound/adc.h"
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
''')
    mark='class s3000_adc_device : public device_t, public device_sound_interface'
    start=s.index(mark)
    end=s.index('DEFINE_DEVICE_TYPE(S3000_ADC',start)
    k=s[start:end]
    k=one(k, '''uint32_t m_ramp = 0;''', '''// P029 OWNER-ONLY TEST: this is intentionally absent in production builds.
    // WAV source is read once at hardware device startup, PCM fed only on
    // legitimate A/D FIFO pop. Never touches Akai sample RAM directly.
    std::vector<int16_t> m_owner_fixture;
    uint32_t m_owner_fixture_pos = 0;
    bool m_owner_fixture_on = false;
    uint32_t m_ramp = 0;''')
    k=one(k, '''		m_stream = stream_alloc(2, 0, clock() ? clock() : 44100);''', '''        if (const char *p = std::getenv("AKAI_OWNER_PRIVATE_ADC_WAV"))
        {
            std::ifstream source(p, std::ios::binary);
            std::vector<unsigned char> b(
                (std::istreambuf_iterator<char>(source)),
                std::istreambuf_iterator<char>());
            auto le16 = [&](size_t i) -> uint32_t {
                return uint32_t(b[i]) | (uint32_t(b[i+1]) << 8);
            };
            auto le32 = [&](size_t i) -> uint32_t {
                return uint32_t(b[i]) | (uint32_t(b[i+1]) << 8)
                     | (uint32_t(b[i+2]) << 16) | (uint32_t(b[i+3]) << 24);
            };
            // Test-only requirement: strict standard PCM16 stereo 44.1k WAV
            // with 44-byte header. Fail closed on unknown RIFF chunks.
            if (b.size() < 44 || b.size() > (44100u * 4u * 12u + 44u)
                || std::memcmp(b.data(), "RIFF", 4)
                || std::memcmp(b.data()+8, "WAVEfmt ", 8)
                || std::memcmp(b.data()+36, "data", 4)
                || le16(20)!=1 || le16(22)!=2 || le32(24)!=44100
                || le16(34)!=16 || le32(40)!=b.size()-44
                || ((b.size()-44)%4)!=0)
                fatalerror("OWNER PRIVATE ADC WAV needs PCM16 stereo 44100 WAV 44-byte header: %s",p);
            m_owner_fixture.reserve((b.size()-44)/2);
            for (size_t off=44; off<b.size();off+=2)
                m_owner_fixture.push_back(int16_t(uint16_t(le16(off))));
            m_owner_fixture_on = true;
            logerror("OWNER_ADC_PCM_LOADED frames=%zu (ONLY converter fixture, NOT VST3 WAV source)\\n",
                     m_owner_fixture.size()/2);
        }
		m_stream = stream_alloc(2, 0, clock() ? clock() : 44100);''')
    k=one(k, '''		m_rd++;

#if S3000_ADC_RAMP''','''		m_rd++;
        if (m_owner_fixture_on)
        {
            // Consume the real ADC FIFO in the ordinary path above. A WAV
            // frame replaces ONLY the converter word handed to the Akai DMA.
            // This preserves original WADCS/DMRQ/firmware record timing.
            const size_t at = size_t(m_owner_fixture_pos++)*2;
            if (at+1 < m_owner_fixture.size()) {
                l = m_owner_fixture[at];
                r = m_owner_fixture[at+1];
            } else l = r = 0;
            m_last_l = l;
            m_last_r = r;
        }

#if S3000_ADC_RAMP''')
    s=s[:start]+k+s[end:]
    path.write_text(s,encoding='utf-8')
    print('OWNER_ONLY_AKAI_ADC_WAV_CONVERTER_OVERLAY=PASS')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('patched_driver',type=Path)
    a=p.parse_args()
    patch(a.patched_driver)

if __name__=='__main__':
    main()
