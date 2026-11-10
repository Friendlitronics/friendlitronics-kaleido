# Friendlitronics Kaleido

A stereo multi-effects plugin: four reorderable slots, one effect active in
each, and one **Drift** control that makes the whole chain wander, warble and
glitch. **AU + VST3 + Standalone**, macOS, built with JUCE and CMake. Same
family as Friendlitronics Amp and Friendlitronics Tape.

It works at instrument level (guitar or bass plugged straight in) and at line
level (synths, vocals, buses), with an input calibration stage so the
level-dependent effects behave the same either way.

> Status: **milestone 1** (scaffold and slot engine). Every effect is ported
> from Friendlitronics Amp; the new effects and the real UI come later. See
> [docs/TODO.md](docs/TODO.md).

## The four slots

Working names. Every slot has **Amount / Tone / Mix** plus a fourth knob whose
job depends on the effect.

| Slot | Effects | Amount | Tone | Mix | 4th knob |
|---|---|---|---|---|---|
| **Shape** | Screamer, Fuzz, Sustainer | drive / sustain | tone (Sustainer: attack) | dry/wet | level |
| **Motion** | Phaser, Chorus, Dive | depth (Dive: pitch, 5 = unison) | feedback / tone / double | dry/wet | rate (Dive: glide) |
| **Space** | Delay, Spring, Room | repeats / decay / size | tone | dry/wet | time (reverbs: width) |
| **Grain** | Tape, LoFi | saturate / crush | tone (LoFi: decimate) | dry/wet | flutter / noise |

Knob positions belong to the slot, not the effect, so switching from Delay to
Spring keeps the knobs where they are.

## Drift

One global knob feeding one shared random source, so the whole chain misbehaves
together:

- **wander**: a slow random walk (a new direction every 0.4 to 2.5 s)
- **warble**: an irregular 4 to 7 Hz wobble, like an unsteady motor
- **glitch**: occasional events, none below about 15% and roughly one every
  two seconds at full

Each effect decides what those mean. The delay's time wanders and warbles like
a tired transport, Dive's pitch drifts and above 40% jumps an octave on a
glitch, Tape drops out, LoFi crashes its sample rate, and the drives sag as if
the battery were dying. The full list is in `Source/engine/FxUnits.h`.

## Input calibration

The effects were voiced in the amp for a guitar DI. **INPUT: Line** pads the
chain by 10 dB and restores it afterwards, so a synth or a bus hits the drives
and the tape the way a guitar would and still comes out at line level. **TRIM**
(±12 dB) sets how hard the chain is hit; aim the IN meter at the middle.

## Build

```bash
# JUCE: a local checkout or symlink at ./JUCE is used if present; otherwise
# CMake fetches 7.0.12 and applies the macOS 15 patch in patches/.
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Headless engine test (every effect in every slot, reorder, Drift):
cmake --build build --target KaleidoEngineTest
./build/KaleidoEngineTest_artefacts/Release/KaleidoEngineTest

# Validate the Audio Unit (after a desk build, which installs it):
auval -v aufx Kald Frnd
```

A desk build copies the AU and VST3 into `~/Library/Audio/Plug-Ins`. Set `CI=1`
in the environment to skip that. The Standalone is at
`build/FriendlitronicsKaleido_artefacts/Release/Standalone/`.

Signed and notarised releases use `scripts/sign-and-notarise.sh` locally, or
CI on a `v*` tag once the signing secrets are set (`scripts/setup-ci-signing.sh`).
This is the same setup as Friendlitronics Tape.

## Layout

```
Source/
  dsp/            ported DSP (BiquadHelpers, Waveshapers, InputStage, TapeColour, LoFiStage)
  dsp/fx/         ported pedals: Drive, Sustainer, TimeFx (chorus/phaser/delay), Dive, Reverbs
  engine/         Catalogue (categories, effects, labels), Drift, FxUnits (adapters),
                  Slot, KaleidoEngine
  ui/             look-and-feel, chain strip, slot cards, meters
  PluginProcessor / PluginEditor
tests/EngineTest.cpp
```

## Licence

GPL-3.0-or-later, as with the other Friendlitronics plugins. See `LICENSE`.
