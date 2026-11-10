# Friendlitronics Kaleido: developer notes

_Last updated: 2026-11-09 (v0.1.0, milestone 1)_

## Where things came from
- **Scaffold** (CMake, JUCE fetch and patch, `cmake/ApplyPatch.cmake`, signing
  scripts, CI) is from `~/repos/tape_emulator`. The signing script also signs the
  Standalone, with the audio-input entitlement, the same way Tape signs the Deck app.
- **DSP** is from `~/repos/amp_sim`, copied rather than shared, with the namespace
  renamed to `kaleido`. Changes from the amp:
  - `fx/TimeFx.h`: Chorus gained `setTone` (wet bandwidth 2-9 kHz). Delay gained
    `setModulation` (a fast-smoothed time offset for Drift).
  - `fx/Dive.h`: gained `setGlide` (20 ms to 2 s) and `setDetune` (cents, for Drift).
  - `fx/Reverbs.h`: the amp's SpringReverb and RoomSim became one Freeverb wrapper
    with two voicings and size, tone and width controls. At mid settings they
    match the amp.
  - `fx/Sustainer.h`: the Wah was dropped (not in Kaleido's list).
  - `InputStage.h`: the amp's version was a coupling high-pass plus a per-amp
    bright cap. This one keeps the high-pass (18 Hz) and adds the Instrument/Line
    calibration (a 10 dB pad, undone at the output) and trim.

## How the engine works
- `Catalogue.h` lists the categories, effects, knob labels and defaults. Add
  effects at the **end** of a category's list, because the list index is the
  saved choice value.
- `FxUnits.h` has one adapter per effect. Each adapter maps Amount, Tone and
  Extra onto the ported DSP and decides how that effect responds to Drift.
  Effects run fully wet; the slot does the Mix.
- `Slot.h` holds every effect of a category, prepared up front. Switching
  effect fades to dry over 8 ms, swaps and resets the new effect, then fades
  back in. A slot that has faded fully off stops processing and is reset
  before it is heard again.
- `KaleidoEngine.h` runs input, then the 4 slots in chain order, then the output
  pad undo, in 32-sample chunks. Drift advances once per chunk. Reordering
  ducks for about 6 ms each way.
- Chain order is saved as a `chainOrder` property on the APVTS state ("0,1,2,3")
  and mirrored into a packed atomic for the audio thread, as in the amp.

## Build and test
```bash
CI=1 cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64   # quick dev build, no install
CI=1 cmake --build build -j 10
cmake --build build --target KaleidoEngineTest && ./build/KaleidoEngineTest_artefacts/Release/KaleidoEngineTest
open "build/FriendlitronicsKaleido_artefacts/Release/Standalone/Friendlitronics Kaleido.app"
```
`./JUCE` is a symlink to `~/repos/tape_emulator/JUCE` (already patched). It is
gitignored, and a fresh clone fetches JUCE instead.

## Identity
- Bundle id `dev.elegancia.friendlitronicskaleido`, manufacturer `Frnd`, plugin code `Kald`.
- The concept is inspired by a commercial four-category console pedal. Its
  names, branding, effect names and UI are deliberately not used.
