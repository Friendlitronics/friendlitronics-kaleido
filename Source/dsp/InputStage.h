/*
    This file is part of Friendlitronics Kaleido — a stereo multi-effects plugin.
    Ported from Friendlitronics Amp.
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
#include "BiquadHelpers.h"

namespace kaleido
{

/**
    Input calibration: everything downstream was voiced for a guitar DI.

    The ported effects are level-dependent in the way real pedals are (the
    drives clip, the sustainer's threshold and the tape's saturation all assume
    a certain input level), and they were tuned in the amp for a guitar DI,
    peaking around -12 dBFS. A synth, a vocal bus or a mix bus sits roughly
    10 dB hotter, which would turn every Shape effect into fuzz.

    So the input stage does what the amp's coupling stage did (a DC-blocking
    high-pass, minus the amp-specific bright cap) and adds the calibration:

      LEVEL : Instrument = no pad. Line = a 10 dB pad into the chain and the
              same 10 dB back after it, so the effects see instrument level
              and the output comes back at line level.
      TRIM  : +/-12 dB on top, for hot pickups or quiet sources. Not undone at
              the output, since it is meant to change how hard the chain is hit.

    Gains are smoothed, so flipping the switch while playing does not click.
*/
class InputStage
{
public:
    enum class Level { Instrument = 0, Line };

    static constexpr float kLinePadDb = 10.0f;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        hp.prepare (sampleRate);
        hp.setHighpass (18.0, 0.707);           // coupling cap: DC and rumble only
        inGain.reset (sampleRate, 0.03);
        outGain.reset (sampleRate, 0.03);
        update (true);
    }

    void reset() { hp.reset(); }

    void setLevel (Level l, float trimDbIn)
    {
        level = l;
        trimDb = juce::jlimit (-24.0f, 24.0f, trimDbIn);
        update (false);
    }

    /** Pad + trim + DC block, before the slots. */
    void processInput (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = inGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                block.setSample (ch, i, hp.processSample (ch, block.getSample (ch, i)) * g);
        }
    }

    /** Undoes the Line pad after the slots. */
    void processOutput (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) block.getNumChannels();
        const int numSamples = (int) block.getNumSamples();
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = outGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
                block.setSample (ch, i, block.getSample (ch, i) * g);
        }
    }

private:
    void update (bool snap)
    {
        const float pad = (level == Level::Line) ? kLinePadDb : 0.0f;
        const float in  = dbToGain (trimDb - pad);
        const float out = dbToGain (pad);
        if (snap)
        {
            inGain.setCurrentAndTargetValue (in);
            outGain.setCurrentAndTargetValue (out);
        }
        else
        {
            inGain.setTargetValue (in);
            outGain.setTargetValue (out);
        }
    }

    double sampleRate = 44100.0;
    Biquad hp;
    Level level = Level::Instrument;
    float trimDb = 0.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inGain { 1.0f }, outGain { 1.0f };
};

} // namespace kaleido
