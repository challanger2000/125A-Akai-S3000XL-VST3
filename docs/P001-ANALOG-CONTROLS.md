# P001 — S3000XL physical input/output knob implementation

## Source and design
- Original Akai S3000XL Operator's Manual p. 18: **REC GAIN** (analogue input) and **MAIN VOLUME** (main stereo/headphone output only, not eight individual outputs/digital output). Reference: https://www.firstpr.com.au/rwi/smem/akai-manuals/Akai_S3000XL_Manual.pdf
- The upstream README misnames the second potentiometer as "REC VOLUME". The original machine does not have a second record-volume knob.
- MAME 0.288 `src/mame/layout/s3000xl.lay` defines the actual knob artwork and coordinates, used for hit test and lightweight value marker.
- Existing APVTS `volume` stays a MIDI CC7 control and is **not repurposed**.

## Implementation (development branch)
- `rec_gain`: applies before the MAME microphone/A-D path. 0 is mute, 1 is the accepted upstream 1:1 recording baseline; 5ms gain smoothing.
- `main_volume`: applies after the original MAME sound path and any existing anti-idle samples to Main stereo L/R only. OUT 1..8 are unchanged; 5ms smoothing.
- Both values live in per-instance APVTS host state. The physical knobs accept drag, wheel and Ctrl+left-click to reset to the true default; host edit gestures are emitted for DAW automation.
- UI hit zones derived from the actual original MAME 0.288 panel layout, not guessed from screenshot.
- No new real-time heap allocation, locking or file I/O was added by these gain stages.

## Fidelity boundary / deliberately not claimed
- The analogue potentiometer's exact voltage/taper law and record headroom are **not documented/calibrated**. The initial 0..1 curve is a conservative output/input attenuation implementation; it must not be described as a measured original analogue preamp. Preserve upstream 1:1 at default.
- The firmware's **Mon** soft key is a *different, original firmware-controlled recording/meter-monitoring mode*, not a physical second volume knob; its incomplete audio-through behavior is a separate follow-up.
- Genuine ROM boot, firmware sampling, the LCD, actual GUI mouse operation and DAW project recall have **not** been tested in this change. No VST3 compilation, validator or Studio Pro acceptance is asserted.

## Targeted tests
- `tests/AkaiAnalogControlsTests.cpp` verifies endpoints, neutral/default, stereo/polarity, nonfinite input and a non-zippering 5ms gain ramp.
- Additional required Windows host integration: record sine at knob 0/0.5/1 via original firmware and compare stored PCM; Main at 0/0.5/1 with eight simultaneous individual outs unchanged; host automation, Ctrl-reset, real GUI wheel/drag and save/reload.
- Firmware Mon remains an OPEN requirement until separately implemented and measured with original V2.0 ROM.
