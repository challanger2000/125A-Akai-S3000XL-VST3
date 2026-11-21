#!/usr/bin/env python3
"""Private Akai original-ROM MIDI UART-to-PCM test: no licensed ROMs in repo."""
import argparse
import hashlib
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import wave

HEADER=bytes.fromhex('4d546864000000060000000101e0')
TEMPO=bytes.fromhex('00ff510307a120')
FOOTER=bytes.fromhex('00ff2f00')
def vlq(n):
    a=[n&127];n>>=7
    while n:
        a.insert(0,(n&127)|128);n>>=7
    return bytes(a)

def midi_file(velocity):
    ev=bytearray(TEMPO)
    for i,note in enumerate((60,64,72)):
        ev.extend(b'\x00'+bytes((0x90,note,velocity)))
        ev.extend(vlq(480)+bytes((0x80,note,0)))
        if i!=2: ev.extend(vlq(240))
    ev.extend(FOOTER)
    return HEADER+b'MTrk'+struct.pack('>I',len(ev))+ev

def write_lua(path,state,midi):
    def quote(s):
        return '"'+str(s).replace('\\','\\\\').replace('"','\\"')+'"'
    source='''local restored=false
local base=nil
local loaded=false
local shutdown=false
local state=%s
local midi=%s
emu.register_periodic(function()
  local t=emu.time()
  if not restored and t>=1 then
    restored=true
    manager.machine:load(state)
    emu.print_info('OWNER_ROM_STATE_REQUESTED')
  end
  if restored and not base and t>20 then
    base=t
    emu.print_info(string.format('OWNER_ROM_STATE_ACTIVE_AT %%.4f',t))
  end
  if base and midi~='' and t>=base+0.35 and not loaded then
    loaded=true
    local image=manager.machine.images[':mdin:midiin:midiinimg']
    if not image then error('ORIGINAL_MAME_MIDI_INPUT_IMAGE_ABSENT') end
    local err=image:load(midi)
    if err then error('ORIGINAL_MAME_MIDI_IMAGE_LOAD_FAILURE '..tostring(err)) end
    emu.print_info('REAL_AKAI_MIDI_IMAGE_LOADED')
  end
  if base and t>=base+8.2 and not shutdown then
    shutdown=true
    emu.print_info('OWNER_ROM_TEST_END')
    manager.machine:exit()
  end
end)
''' % (quote(state),quote(midi) if midi else "''")
    path.write_text(source,encoding='utf8')

def wav_info(path):
    with wave.open(str(path),'rb') as w:
        if w.getsampwidth()!=2: raise RuntimeError('expected original MAME PCM16')
        channels,frames,rate=w.getnchannels(),w.getnframes(),w.getframerate()
        data=w.readframes(frames)
    nz=peak=sq=0;counts=[0]*channels
    for i,(v,) in enumerate(struct.iter_unpack('<h',data)):
        if v: nz+=1;counts[i%channels]+=1
        peak=max(peak,abs(v));sq+=v*v
    return dict(channels=channels,frames=frames,rate=rate,
                nonzero=nz,per_channel_nonzero=counts,peak=peak,
                rms=round(math.sqrt(sq/(frames*channels)),4),
                pcm_sha256=hashlib.sha256(data).hexdigest())

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mame',type=Path,required=True)
    p.add_argument('--rompath',type=Path,required=True)
    p.add_argument('--ready-state',type=Path,required=True)
    p.add_argument('--outdir',type=Path,required=True)
    p.add_argument('--linux-sdl-ttf-shim',type=Path)
    a=p.parse_args()
    for f in (a.mame,a.ready_state):
        if not f.is_file(): raise RuntimeError('missing local prerequisite '+str(f))
    a.outdir.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy()
    env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    if a.linux_sdl_ttf_shim:
        env['LD_LIBRARY_PATH']=str(a.linux_sdl_ttf_shim)+':'+env.get('LD_LIBRARY_PATH','')
    results={}
    for label,velocity in (('no_midi',None),('velocity_zero',0),
                           ('midi_velocity_110_a',110),('midi_velocity_110_b',110)):
        midi=a.outdir/(label+'.mid') if velocity is not None else None
        if midi: midi.write_bytes(midi_file(velocity))
        lua=a.outdir/(label+'.lua');wav=a.outdir/(label+'.wav')
        write_lua(lua,a.ready_state.resolve(),midi.resolve() if midi else None)
        cmd=[str(a.mame.resolve()),'s3000xl','-rompath',str(a.rompath.resolve()),
             '-bios','0','-video','none','-sound','none','-nothrottle',
             '-skip_gameinfo','-seconds_to_run','90',
             '-wavwrite',str(wav),'-autoboot_script',str(lua)]
        p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                         text=True,env=env,timeout=130)
        (a.outdir/(label+'.log')).write_text(p.stdout,encoding='utf8')
        if p.returncode!=0 or 'OWNER_ROM_TEST_END' not in p.stdout:
            raise RuntimeError(label+' emulator failed '+p.stdout[-1000:])
        results[label]=wav_info(wav)
        print(label+': '+str(results[label]),flush=True)
    no,zero,x,y=(results[z] for z in
                 ('no_midi','velocity_zero','midi_velocity_110_a','midi_velocity_110_b'))
    assert no['nonzero']==zero['nonzero']==0,'both negative controls must be silent'
    assert x['nonzero']>1000 and x['peak']>100,'original MIDI voice output absent'
    # MAME original Akai stereo is 0-based output channels 1 and 2, NOT 0/1.
    assert x['per_channel_nonzero'][1]>0 and x['per_channel_nonzero'][2]>0,\
           'original hardware stereo channels 1 and 2 not audible'
    assert x['pcm_sha256']==y['pcm_sha256'] and x['frames']==y['frames'],\
           'two independent original firmware MIDI PCM renders are not bit-identical'
    print('PASS original owner-ROM MIDI input, sound PCM, silent negative controls and deterministic replay. Not WAV import/Windows VST3 proof.',flush=True)

if __name__=='__main__':
    try: main()
    except Exception as exc: print('FAIL: '+str(exc),file=sys.stderr);sys.exit(1)
