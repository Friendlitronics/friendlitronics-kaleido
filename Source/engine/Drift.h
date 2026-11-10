/*
    This file is part of Friendlitronics Kaleido — a stereo multi-effects plugin.
    Copyright (C) 2026 David Zevenbergen

    Friendlitronics Kaleido is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version. It is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See <https://www.gnu.org/licenses/> for the full licence text.

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace kaleido
{

/**
    One control-rate snapshot of the shared instability source. Every slot sees
    the same snapshot, so when Drift kicks, the whole chain kicks together, the
    way one failing power supply would make a whole pedalboard misbehave.

    Each effect decides what the signals *mean* for it (FxUnits.h).
*/
struct DriftState
{
    float depth  = 0.0f;   // the DRIFT knob, 0..1: scale everything by this
    float wander = 0.0f;   // slow random walk, -1..1, seconds per move
    float warble = 0.0f;   // fast irregular wobble, -1..1, a few Hz
    float glitch = 0.0f;   // envelope of the current glitch event, 0..1
    float glitchSign = 1.0f;  // +1 or -1, chosen per event, for effects that need a direction
};

/**
    Drift: slow wander, warble and occasional glitch from one random source.

    - **Wander** is a smoothed random walk: a new target every 0.4-2.5 s, reached
      through two one-pole stages, so it never has corners.
    - **Warble** is two incommensurate sines (4.1 and 6.7 Hz) whose amplitude is
      itself wandering, plus a little smoothed noise. It reads as an unsteady
      motor, not a clean LFO.
    - **Glitch** events arrive at random, more often as the knob goes up
      (roughly never below 15%, about one every two seconds at full). Each one
      jumps to 1 and decays over 40-250 ms.

    Runs at control rate (advance() once per engine chunk of <= 32 samples) and
    is seeded, so a render with the same settings is repeatable.
*/
class DriftSource
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        random.setSeed (0x4B414C45);   // "KALE"
        state = {};
        wanderTarget = wanderStage1 = wanderStage2 = 0.0f;
        ampTarget = ampStage = 0.5f;
        warblePhaseA = warblePhaseB = 0.0f;
        warbleNoise = 0.0f;
        secondsToRetarget = 0.0f;
        secondsToRetargetAmp = 0.0f;
        glitchDecay = 0.0f;
    }

    void setAmount (float depth01) { depth = juce::jlimit (0.0f, 1.0f, depth01); }

    /** Advances by `numSamples` and returns the new snapshot. */
    const DriftState& advance (int numSamples)
    {
        const float dt = (float) numSamples / (float) sampleRate;
        const auto twoPi = juce::MathConstants<float>::twoPi;

        // --- wander ---
        secondsToRetarget -= dt;
        if (secondsToRetarget <= 0.0f)
        {
            wanderTarget = random.nextFloat() * 2.0f - 1.0f;
            secondsToRetarget = 0.4f + 2.1f * random.nextFloat();
        }
        const float kWander = 1.0f - std::exp (-dt / 0.6f);
        wanderStage1 += (wanderTarget - wanderStage1) * kWander;
        wanderStage2 += (wanderStage1 - wanderStage2) * kWander;

        // --- warble ---
        secondsToRetargetAmp -= dt;
        if (secondsToRetargetAmp <= 0.0f)
        {
            ampTarget = 0.2f + 0.8f * random.nextFloat();
            secondsToRetargetAmp = 0.3f + 1.2f * random.nextFloat();
        }
        ampStage += (ampTarget - ampStage) * (1.0f - std::exp (-dt / 0.25f));

        warblePhaseA += twoPi * 4.1f * dt; if (warblePhaseA >= twoPi) warblePhaseA -= twoPi;
        warblePhaseB += twoPi * 6.7f * dt; if (warblePhaseB >= twoPi) warblePhaseB -= twoPi;
        const float white = random.nextFloat() * 2.0f - 1.0f;
        warbleNoise += (white - warbleNoise) * (1.0f - std::exp (-dt / 0.03f));
        const float warble = ampStage * (0.6f * std::sin (warblePhaseA) + 0.3f * std::sin (warblePhaseB))
                           + 0.25f * warbleNoise;

        // --- glitch ---
        if (state.glitch > 1.0e-4f)
            state.glitch *= std::exp (-dt / glitchDecay);
        else
            state.glitch = 0.0f;

        if (depth > 0.15f)
        {
            const float g = (depth - 0.15f) / 0.85f;
            const float eventsPerSecond = 0.55f * g * g;
            if (random.nextFloat() < eventsPerSecond * dt && state.glitch < 0.2f)
            {
                state.glitch = 1.0f;
                state.glitchSign = random.nextBool() ? 1.0f : -1.0f;
                glitchDecay = 0.04f + 0.21f * random.nextFloat();
            }
        }

        state.depth  = depth;
        state.wander = juce::jlimit (-1.0f, 1.0f, wanderStage2);
        state.warble = juce::jlimit (-1.0f, 1.0f, warble);
        return state;
    }

    const DriftState& current() const { return state; }

private:
    double sampleRate = 44100.0;
    float depth = 0.0f;
    DriftState state;
    juce::Random random;

    float wanderTarget = 0.0f, wanderStage1 = 0.0f, wanderStage2 = 0.0f, secondsToRetarget = 0.0f;
    float ampTarget = 0.5f, ampStage = 0.5f, secondsToRetargetAmp = 0.0f;
    float warblePhaseA = 0.0f, warblePhaseB = 0.0f, warbleNoise = 0.0f;
    float glitchDecay = 0.1f;
};

} // namespace kaleido
