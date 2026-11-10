# Kaleido TODO

_Last updated: 2026-11-09 (milestone 1)_

## Decisions waiting on DJ
- [ ] **Final effect list per category.** Right now Shape, Motion and Space have
      3 effects each and Grain has 2, all ported from the amp. The plan has 5 per
      category; candidates are listed under "Later" below.
- [ ] **Category names.** Shape / Motion / Space / Grain are working names. Display
      names live in `Source/engine/Catalogue.h`. Parameter ids use the lowercase
      tags (`shape`, `motion`, ...), which can stay put even if the display names
      change.
- [ ] **UI direction.** The current editor is a functional placeholder.

## P1: known issues in milestone 1
- [ ] **Tape flutter plus a partial Grain Mix combs.** TapeColour's flutter delay adds
      about 1.5 ms that the slot's dry path does not have, so Mix between 0 and 10
      with Flutter up gives a static comb. Fix: let a unit report its delay so the
      slot can delay the dry signal to match.
- [ ] **No oversampling.** In the amp, the drives ran inside 4x oversampling. Here
      they run at the host rate and rely on ADAA. Fuzz and Screamer at high drive
      will alias more than they did in the amp. Option: 2x oversample the Shape
      slot only, and report the latency.
- [ ] **Level knob range.** The Shape 4th knob gives ±12 dB (the amp's pedal range),
      so Screamer or Sustainer at 10 can peak around +12 dBFS. Consider ±6 dB.
- [ ] **Reverbs ignore glitch.** Spring and Room only respond to wander and warble.
      Idea: a glitch kicks the tank (a short burst into the reverb input).
- [ ] **Zipper on fast Drift.** Effects retune filters once per 32-sample chunk.
      That is fine for wander, but check the phaser and tape tone under full warble.
- [ ] **Run `auval`** after a desk build installs the AU. It has not been run yet
      because the milestone build used `CI=1`.

## Later (not milestone 1)
- New effects: howl/resonant fuzz, envelope swell, tremolo, vibrato/warble,
  doubler, tape echo, reverse delay, big reverb, multimode filter,
  "broken motor", radio interference.
- Factory presets (needs the effect list settled first).
- v2: gesture (knob-motion) recording, capture looper.
