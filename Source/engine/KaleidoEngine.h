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

#include "Slot.h"
#include "../dsp/InputStage.h"

namespace kaleido
{

/**
    The whole signal path, independent of the plugin wrapper (so the headless
    test can drive it):

        input calibration -> slot -> slot -> slot -> slot -> undo Line pad

    with the four slots in the user's chain order and one Drift source shared by
    all of them. Audio is processed in chunks of up to 32 samples; Drift
    advances once per chunk, which is fast enough for a 7 Hz warble and cheap
    enough to retune filters at.

    Reordering ducks the chain output to silence over ~6 ms, swaps the order at
    the bottom, and brings it back. Crossfading two orders would mean running
    the chain twice, which is not worth it for something done by hand.

    No oversampling, so no latency. The drive effects rely on their ADAA
    shapers instead (see docs/TODO.md).
*/
class KaleidoEngine
{
public:
    struct SlotParams
    {
        bool on = false;
        int effect = 0;
        Knobs knobs;
    };

    struct Params
    {
        std::array<SlotParams, (size_t) kNumSlots> slots {};   // indexed by Category
        ChainOrder order = defaultChainOrder();
        float drift = 0.0f;                                     // 0..1
        InputStage::Level inputLevel = InputStage::Level::Instrument;
        float inputTrimDb = 0.0f;
    };

    KaleidoEngine()
        : slots { { std::make_unique<Slot> (Category::Shape),
                    std::make_unique<Slot> (Category::Motion),
                    std::make_unique<Slot> (Category::Space),
                    std::make_unique<Slot> (Category::Grain) } }
    {
    }

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlockSize),
                                      (juce::uint32) juce::jlimit (1, 2, numChannels) };
        input.prepare (spec);
        for (auto& s : slots)
            s->prepare (spec);
        drift.prepare (sampleRate);
        duckStep = (float) (1.0 / (0.006 * sampleRate));
        reset();
    }

    void reset()
    {
        input.reset();
        for (auto& s : slots)
            s->reset();
        drift.reset();
        order = pendingOrder;
        duck = 1.0f;
    }

    void setParams (const Params& p)
    {
        for (int c = 0; c < kNumSlots; ++c)
        {
            const auto& sp = p.slots[(size_t) c];
            slots[(size_t) c]->set (sp.effect, sp.on, sp.knobs);
        }
        if (chainOrderValid (p.order))
            pendingOrder = p.order;
        drift.setAmount (p.drift);
        input.setLevel (p.inputLevel, p.inputTrimDb);
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        input.processInput (block);

        const int numSamples = (int) block.getNumSamples();
        chainInputPeak = 0.0f;
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            for (int i = 0; i < numSamples; ++i)
                chainInputPeak = juce::jmax (chainInputPeak, std::abs (block.getSample ((int) ch, i)));
        for (int start = 0; start < numSamples; start += Slot::kMaxChunk)
        {
            const int n = juce::jmin (Slot::kMaxChunk, numSamples - start);
            auto chunk = block.getSubBlock ((size_t) start, (size_t) n);

            const auto& d = drift.advance (n);
            for (int position = 0; position < kNumSlots; ++position)
                slots[(size_t) order[(size_t) position]]->process (chunk, d);

            applyReorderDuck (chunk);
        }

        input.processOutput (block);
    }

    const DriftState& driftState() const { return drift.current(); }

    /** Peak of the last block as the effects saw it (after pad and trim): what
        the input meter shows, since that is the level the calibration is for. */
    float lastChainInputPeak() const { return chainInputPeak; }
    const ChainOrder& currentOrder() const { return order; }

private:
    void applyReorderDuck (juce::dsp::AudioBlock<float>& chunk)
    {
        const bool reordering = (pendingOrder != order);
        if (! reordering && duck >= 1.0f)
            return;

        const int n = (int) chunk.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            duck = reordering ? juce::jmax (0.0f, duck - duckStep)
                              : juce::jmin (1.0f, duck + duckStep);
            for (size_t ch = 0; ch < chunk.getNumChannels(); ++ch)
                chunk.setSample ((int) ch, i, chunk.getSample ((int) ch, i) * duck);
        }

        if (reordering && duck <= 0.0f)
            order = pendingOrder;
    }

    InputStage input;
    std::array<std::unique_ptr<Slot>, (size_t) kNumSlots> slots;
    DriftSource drift;

    ChainOrder order = defaultChainOrder(), pendingOrder = defaultChainOrder();
    float duck = 1.0f, duckStep = 0.004f;
    float chainInputPeak = 0.0f;
};

} // namespace kaleido
