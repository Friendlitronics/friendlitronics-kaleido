/*
    This file is part of Friendlitronics Kaleido — a stereo multi-effects plugin.
    Ported from Friendlitronics Amp (SpringReverb.h and RoomSim.h).
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
#include "../BiquadHelpers.h"

namespace kaleido::fx
{

/**
    The two small reverbs from the amp: both are juce::dsp::Reverb (Freeverb)
    voiced differently. In the amp each had one knob; here each gets SIZE,
    TONE and WIDTH, with the amp's settings at the middle of the ranges.

    Both output dry + wet (dry at unity), so the slot's MIX crossfade becomes
    "how much reverb" without dipping the direct sound.
*/
class FreeverbVoice
{
public:
    struct Voicing
    {
        float sizeMin, sizeMax;        // roomSize range swept by SIZE
        float dampDark, dampBright;    // damping at TONE = 0 and TONE = 1
        float wetLevel;                // the amp's wet level at full Room/Reverb
    };

    explicit FreeverbVoice (Voicing v) : voicing (v) {}

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        reverb.prepare (spec);
        update();
    }

    void reset() { reverb.reset(); }

    void setControls (float size01, float tone01, float width01)
    {
        if (! moved (size01, size) && ! moved (tone01, tone) && ! moved (width01, width))
            return;
        size = size01; tone = tone01; width = width01;
        update();
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

private:
    void update()
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize   = juce::jmap (juce::jlimit (0.0f, 1.0f, size), voicing.sizeMin, voicing.sizeMax);
        p.damping    = juce::jmap (juce::jlimit (0.0f, 1.0f, tone), voicing.dampDark, voicing.dampBright);
        p.width      = juce::jlimit (0.0f, 1.0f, width);
        p.freezeMode = 0.0f;
        p.dryLevel   = 1.0f;
        p.wetLevel   = voicing.wetLevel;
        reverb.setParameters (p);
    }

    Voicing voicing;
    juce::dsp::Reverb reverb;
    float size = 0.5f, tone = 0.5f, width = 1.0f;
};

/** The Twin's tank: brighter and longer than the room. The amp ran it at
    roomSize 0.6 / damping 0.35, which sits mid-range here. */
inline FreeverbVoice::Voicing springVoicing() { return { 0.40f, 0.85f, 0.60f, 0.10f, 0.40f }; }

/** The ambient room mic: small and dark. The amp ran it at roomSize 0.32 /
    damping 0.7. */
inline FreeverbVoice::Voicing roomVoicing()   { return { 0.15f, 0.50f, 0.90f, 0.50f, 0.50f }; }

} // namespace kaleido::fx
