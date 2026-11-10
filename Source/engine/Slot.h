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

#include "FxUnits.h"

namespace kaleido
{

/**
    One category's slot: every effect in the category, of which one is active,
    plus the slot's on/off and dry/wet.

    All of the category's effects are built and prepared up front, so picking a
    different effect never allocates on the audio thread. Changes are made
    smooth rather than instant:

      - **Switching effect** fades the slot to dry over ~8 ms, swaps (resetting
        the incoming effect so it never plays a stale tail), and fades back in.
      - **On/off and Mix** are a smoothed crossfade. Once a slot has faded fully
        off it stops processing, and the effect is reset before it is next
        heard, so a delay switched back on starts clean.

    Process in chunks no longer than kMaxChunk.
*/
class Slot
{
public:
    static constexpr int kMaxChunk = 32;

    explicit Slot (Category c) : category (c), units (makeUnits (c)) {}

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        juce::dsp::ProcessSpec s = spec;
        s.maximumBlockSize = (juce::uint32) kMaxChunk;
        for (auto& u : units)
            u->prepare (s);

        mix.reset (sampleRate, 0.02);
        mix.setCurrentAndTargetValue (targetMix());
        fadeStep = (float) (1.0 / (0.008 * sampleRate));
        reset();
    }

    void reset()
    {
        for (auto& u : units)
            u->reset();
        active = requested;
        fade = 1.0f;
        needsReset = false;
        mix.setCurrentAndTargetValue (targetMix());
    }

    void set (int effectIndex, bool isOn, const Knobs& k)
    {
        requested = juce::jlimit (0, (int) units.size() - 1, effectIndex);
        on = isOn;
        knobs = k;
        mix.setTargetValue (targetMix());
    }

    Category getCategory() const { return category; }
    int  activeEffect() const    { return active; }

    /** True while the slot contributes anything (including fading out). */
    bool isAudible() const { return mix.isSmoothing() || mix.getTargetValue() > 0.0f; }

    void process (juce::dsp::AudioBlock<float>& block, const DriftState& drift)
    {
        const int numSamples = (int) block.getNumSamples();
        const int numCh = (int) juce::jmin ((size_t) kMaxChannels, block.getNumChannels());
        jassert (numSamples <= kMaxChunk);

        if (! isAudible())
        {
            // Fully off. Skip the work and remember to clear the effect's
            // memory before it is heard again.
            needsReset = true;
            active = requested;
            fade = 1.0f;
            return;
        }

        if (needsReset)
        {
            units[(size_t) active]->reset();
            needsReset = false;
        }

        for (int ch = 0; ch < numCh; ++ch)
            std::copy (block.getChannelPointer ((size_t) ch),
                       block.getChannelPointer ((size_t) ch) + numSamples,
                       dry[(size_t) ch].begin());

        units[(size_t) active]->process (block, knobs, drift);

        for (int i = 0; i < numSamples; ++i)
        {
            // Effect-switch fade: out towards the dry signal, swap at the
            // bottom, back in. The swap lands on a chunk boundary.
            if (active != requested)
                fade = juce::jmax (0.0f, fade - fadeStep);
            else if (fade < 1.0f)
                fade = juce::jmin (1.0f, fade + fadeStep);

            const float m = mix.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float d = dry[(size_t) ch][(size_t) i];
                const float wet = fade * block.getSample (ch, i) + (1.0f - fade) * d;
                block.setSample (ch, i, d + m * (wet - d));
            }
        }

        if (active != requested && fade <= 0.0f)
        {
            active = requested;
            units[(size_t) active]->reset();
        }
    }

private:
    static constexpr int kMaxChannels = 2;

    float targetMix() const { return on ? juce::jlimit (0.0f, 1.0f, knobs.mix) : 0.0f; }

    Category category;
    std::vector<std::unique_ptr<FxUnit>> units;

    double sampleRate = 44100.0;
    int requested = 0, active = 0;
    bool on = false, needsReset = false;
    Knobs knobs;

    float fade = 1.0f, fadeStep = 0.003f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mix;
    std::array<std::array<float, (size_t) kMaxChunk>, (size_t) kMaxChannels> dry {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Slot)
};

} // namespace kaleido
