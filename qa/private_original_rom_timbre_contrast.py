#!/usr/bin/env python3
"""Private original Akai V2 MIDI99 vs factory SINE spectral counterfactual.

Requires separate authentic owner-original-ROM native MAME PCM16 48kHz
11-channel files. Does not upload or store owner ROM, WAV, MIDI or state.
"""
import argparse, math, struct, wave

def ratio(path):
    with wave.open(path,'rb') as w:
        if (w.getsampwidth(),w.getframerate(),w.getnchannels())!=(2,48000,11):
            raise ValueError('Expected original MAME 48kHz 11-channel PCM16')
        frames=w.getnframes()
        data=w.readframes(frames)
        if len(data)!=frames*22:raise ValueError('Truncated original Akai WAV')
    chan=[v[1] for v in struct.iter_unpack('<11h',data)]
    start=next((i for i,v in enumerate(chan) if v),None)
    if start is None:raise ValueError('Silent Akai MIDI C3')
    y=chan[start+2500:start+12500]
    if len(y)!=10000:raise ValueError('Truncated Akai MIDI C3 voice')
    def spectral_peak(begin,end):
        best=(0.,0.)
        for hz2 in range(int(2*begin),int(2*end)+1):
            hz=hz2/2; re=im=0.
            for i,v in enumerate(y):
                theta=2*math.pi*hz*i/48000.
                win=.5-.5*math.cos(2*math.pi*i/(len(y)-1))
                re+=v*win*math.cos(theta)
                im-=v*win*math.sin(theta)
            value=math.hypot(re,im)
            if value>best[0]:best=(value,hz)
        return best
    fundamental=spectral_peak(250,278)
    harmonic=spectral_peak(495,550)
    if fundamental[0]<=1000:raise ValueError('C3 missing original fundamental')
    return {'ratio':harmonic[0]/fundamental[0],
            'base_hz':fundamental[1],'harmonic_hz':harmonic[1],
            'nonzero':sum(bool(v) for v in chan)}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--owner-sds-replay',required=True)
    ap.add_argument('--owner-factory-sine',required=True)
    args=ap.parse_args()
    imported=ratio(args.owner_sds_replay)
    factory=ratio(args.owner_factory_sine)
    print('MIDI99 genuine original Akai',imported)
    print('SINE genuine original Akai',factory)
    assert imported['nonzero']>10000 and factory['nonzero']>10000
    assert imported['ratio']>0.2 and factory['ratio']<0.05
    assert imported['ratio']>15*factory['ratio']
    print('P039 PASS: imported original-ROM PCM timbre differs from factory SINE')

if __name__=='__main__': main()
