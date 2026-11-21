# P039 original-ROM imported-SDS content is not factory SINE

Original Akai S3000XL V2 firmware MIDI C3 sample playback through original DSP. The imported sample is a 44100-frame harmonically-rich WAV brought over real MIDI SDS, assigned to original TEST PROGRAM by physical SINGLE, EDIT, F2, F6, DATAENTRY and saved as genuine native MAME MAMESAVE. The SDS source was not present when the owner state was reopened twice with bit-identical PCM.

To rule out the false-positive hypothesis that MIDI 99 is only a **renamed factory SINE**, compare original owner-ROM 48kHz PCM16 11-channel MIDI note C3 from factory SINE against the restored imported MIDI 99. The independent pure-Python standard-library test is qa/private_original_rom_timbre_contrast.py and never uploads owner PCM/ROM.

Both fundamental peaks are 262.5 Hz after original program transposition. The imported MIDI 99 second harmonic near 525.0Hz has a Hann-window DFT amplitude of 0.89287 relative to fundamental; original factory SINE has 0.00428, an approximately 209-fold contrast. The imported source waveform intentionally has a second harmonic and therefore cannot be reduced to the original factory oscillator.

Run with private local files using python3 qa/private_original_rom_timbre_contrast.py --owner-sds-replay /private/MIDI99.wav --owner-factory-sine /private/SINE.wav.

Strict boundary: original-ROM native MAME proves sample content, but Windows original-ROM VST3 source import, native note playback and DAW save/load are NOT YET proven by this test. Private owner ROM/PCM/native MAMESAVE files are never in the public repository.
