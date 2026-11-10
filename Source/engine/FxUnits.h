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
#include <memory>
#include <vector>

#include "Catalogue.h"
#include "Drift.h"
#include "../dsp/fx/Drive.h"
#include "../dsp/fx/Sustainer.h"
#include "../dsp/fx/TimeFx.h"
#include "../dsp/fx/Dive.h"
#include "../dsp/fx/Reverbs.h"
#include "../dsp/TapeColour.h"
#include "../dsp/LoFiStage.h"

namespace kaleido
{

/** A slot's four knobs, normalised 0..1. Mix is applied by the slot, not the
    effect; it is here so an effect can see it if it ever needs to. */
struct Knobs
{
    float amount = 0.5f, tone = 0.5f, mix = 1.0f, extra = 0.5f;
};

/**
    The uniform face every effect shows the slot: prepare, reset, and process
    a chunk given the slot's knobs and the current Drift snapshot.

    Each unit is a thin adapter over DSP ported from the amp. The adapter is
    where two decisions live: how Amount / Tone / Extra map onto the effect's
    own controls, and how the effect responds to Drift. Effects are always
    run "100% wet" in their own terms; the slot does the dry/wet blend.
*/
class FxUnit
{
public:
    virtual ~FxUnit() = default;
    virtual void prepare (const juce::dsp::ProcessSpec&) = 0;
    virtual void reset() = 0;
    virtual void process (juce::dsp::AudioBlock<float>&, const Knobs&, const DriftState&) = 0;
};

namespace units
{

inline float clamp01 (float x) { return juce::jlimit (0.0f, 1.0f, x); }

/** Gain that ramps linearly across a chunk, so a per-chunk glitch envelope
    becomes a smooth gain curve instead of steps. */
struct ChunkGain
{
    void reset() { last = 1.0f; }

    void apply (juce::dsp::AudioBlock<float>& block, float target)
    {
        const int n = (int) block.getNumSamples();
        if (std::abs (target - 1.0f) < 1.0e-5f && std::abs (last - 1.0f) < 1.0e-5f)
            return;
        const float step = (target - last) / (float) juce::jmax (1, n);
        for (int i = 0; i < n; ++i)
        {
            const float g = last + step * (float) (i + 1);
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                block.setSample ((int) ch, i, block.getSample ((int) ch, i) * g);
        }
        last = target;
    }

    float last = 1.0f;
};

//==============================================================================
// Shape

/** Drift: the drive breathes with wander; a glitch is a starved-supply sag. */
class ScreamerUnit : public FxUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) override { fx.prepare (s); duck.reset(); }
    void reset() override { fx.reset(); duck.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        fx.setControls (clamp01 (k.amount + 0.12f * d.depth * d.wander), k.tone, k.extra);
        fx.process (b);
        duck.apply (b, 1.0f - 0.6f * d.depth * d.glitch);
    }
private:
    fx::Screamer fx;
    ChunkGain duck;
};

/** Drift: sustain wanders; a glitch is the dying-battery sputter. */
class FuzzUnit : public FxUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) override { fx.prepare (s); duck.reset(); }
    void reset() override { fx.reset(); duck.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        fx.setControls (clamp01 (k.amount + 0.15f * d.depth * d.wander), k.tone, k.extra);
        fx.process (b);
        duck.apply (b, 1.0f - 0.9f * d.depth * d.glitch);
    }
private:
    fx::Fuzz fx;
    ChunkGain duck;
};

/** Tone is ATTACK. Drift: the threshold wanders; a glitch is a pump. */
class SustainerUnit : public FxUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) override { fx.prepare (s); duck.reset(); }
    void reset() override { fx.reset(); duck.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        fx.setControls (clamp01 (k.amount + 0.15f * d.depth * d.wander), k.tone, k.extra);
        fx.process (b);
        duck.apply (b, 1.0f - 0.5f * d.depth * d.glitch);
    }
private:
    fx::Sustainer fx;
    ChunkGain duck;
};

//==============================================================================
// Motion

/** Tone is FEEDBACK, Extra is RATE. Drift: rate wanders, depth warbles, and a
    glitch kicks the sweep faster for a moment. */
class PhaserUnit : public FxUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) override { fx.prepare (s); }
    void reset() override { fx.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        const float rate = clamp01 (k.extra * (1.0f + 0.5f * d.depth * d.wander) + 0.4f * d.depth * d.glitch);
        fx.setControls (rate, clamp01 (k.amount + 0.25f * d.depth * d.warble), k.tone);
        fx.process (b);
    }
private:
    fx::Phaser fx;
};

/** Extra is RATE. Drift: depth warbles and rate wanders, the seasick chorus. */
class ChorusUnit : public FxUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) override { fx.prepare (s); }
    void reset() override { fx.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        fx.setControls (clamp01 (k.extra * (1.0f + 0.4f * d.depth * d.wander)),
                        clamp01 (k.amount + 0.35f * d.depth * d.warble), 1.0f);
        fx.setTone (k.tone);
        fx.process (b);
    }
private:
    fx::Chorus fx;
};

/** Amount is PITCH (5 = unison), Tone is DOUBLE, Extra is GLIDE. Drift:
    pitch wanders and warbles by a few tens of cents, and above 40% a glitch
    throws the pitch an octave up or down for a moment. */
class DiveUnit : public FxUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) override { fx.prepare (s); }
    void reset() override { fx.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        float cents = d.depth * (30.0f * d.wander + 10.0f * d.warble);
        if (d.depth > 0.4f)
            cents += d.glitchSign * 1200.0f * d.glitch;
        fx.setControls (k.amount, 1.0f, k.tone);
        fx.setGlide (k.extra);
        fx.setDetune (cents);
        fx.process (b);
    }
private:
    fx::Dive fx;
};

//==============================================================================
// Space

/** Amount is REPEATS, Extra is TIME. Drift: the delay time wanders and warbles
    like a tired tape transport; a glitch surges the feedback. */
class DelayUnit : public FxUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) override { sampleRate = s.sampleRate; fx.prepare (s); }
    void reset() override { fx.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        fx.setControls (k.extra, clamp01 (k.amount + 0.35f * d.depth * d.glitch), 1.0f, k.tone);
        const float ms = d.depth * (5.0f * d.wander + 0.45f * d.warble);
        fx.setModulation ((float) (ms * 0.001 * sampleRate));
        fx.process (b);
    }
private:
    double sampleRate = 44100.0;
    fx::Delay fx;
};

/** Amount is DECAY / SIZE, Extra is WIDTH. Drift: size wanders and the damping
    warbles slightly. No glitch response yet. */
class ReverbUnit : public FxUnit
{
public:
    explicit ReverbUnit (fx::FreeverbVoice::Voicing v) : fx (v) {}
    void prepare (const juce::dsp::ProcessSpec& s) override { fx.prepare (s); }
    void reset() override { fx.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        fx.setControls (clamp01 (k.amount + 0.10f * d.depth * d.wander),
                        clamp01 (k.tone + 0.10f * d.depth * d.warble),
                        0.3f + 0.7f * k.extra);
        fx.process (b);
    }
private:
    fx::FreeverbVoice fx;
};

//==============================================================================
// Grain

/** Amount is SATURATE, Extra is FLUTTER, Tone is a post low-pass (2.5-20 kHz).
    Drift: flutter deepens and saturation wanders; a glitch is a tape dropout
    (level dips and the top end goes with it). */
class TapeUnit : public FxUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) override
    {
        fx.prepare (s);
        toneLp.prepare (s.sampleRate);
        duck.reset();
    }
    void reset() override { fx.reset(); toneLp.reset(); duck.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        const float flutter = clamp01 (k.extra + 0.5f * d.depth * (0.5f + 0.5f * d.warble));
        fx.setControls (clamp01 (k.amount + 0.10f * d.depth * d.wander), flutter);
        fx.process (b);

        const float drop = d.depth * d.glitch;
        const double hz = 2500.0 * std::pow (8.0, (double) k.tone) * (1.0 - 0.75 * drop);
        if (std::abs (hz - lastHz) > 1.0)
        {
            toneLp.setLowpass (hz, 0.707);
            lastHz = hz;
        }
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, b.getNumChannels());
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* x = b.getChannelPointer ((size_t) ch);
            for (size_t i = 0; i < b.getNumSamples(); ++i)
                x[i] = toneLp.processSample (ch, x[i]);
        }
        duck.apply (b, 1.0f - 0.8f * drop);
    }
private:
    TapeColour fx;
    Biquad toneLp;
    double lastHz = -1.0;
    ChunkGain duck;
};

/** Amount is CRUSH, Tone is DECIMATE (sample-rate reduction), Extra is NOISE.
    Drift: crush wanders and a little crackle is added; a glitch crashes the
    sample rate and drops the level, like a buffer underrun. */
class LoFiUnit : public FxUnit
{
public:
    void prepare (const juce::dsp::ProcessSpec& s) override { fx.prepare (s); duck.reset(); }
    void reset() override { fx.reset(); duck.reset(); }
    void process (juce::dsp::AudioBlock<float>& b, const Knobs& k, const DriftState& d) override
    {
        const float hit = d.depth * d.glitch;
        fx.setControls (clamp01 (k.amount + 0.15f * d.depth * d.wander),
                        juce::jmax (k.tone, hit),
                        clamp01 (k.extra + 0.15f * d.depth),
                        0.0f);
        fx.process (b);
        duck.apply (b, 1.0f - 0.6f * hit);
    }
private:
    LoFiStage fx;
    ChunkGain duck;
};

} // namespace units

/** Builds the units for a category, in the order effectsFor() lists them. */
inline std::vector<std::unique_ptr<FxUnit>> makeUnits (Category c)
{
    std::vector<std::unique_ptr<FxUnit>> u;
    switch (c)
    {
        case Category::Shape:
            u.push_back (std::make_unique<units::ScreamerUnit>());
            u.push_back (std::make_unique<units::FuzzUnit>());
            u.push_back (std::make_unique<units::SustainerUnit>());
            break;
        case Category::Motion:
            u.push_back (std::make_unique<units::PhaserUnit>());
            u.push_back (std::make_unique<units::ChorusUnit>());
            u.push_back (std::make_unique<units::DiveUnit>());
            break;
        case Category::Space:
            u.push_back (std::make_unique<units::DelayUnit>());
            u.push_back (std::make_unique<units::ReverbUnit> (fx::springVoicing()));
            u.push_back (std::make_unique<units::ReverbUnit> (fx::roomVoicing()));
            break;
        case Category::Grain:
            u.push_back (std::make_unique<units::TapeUnit>());
            u.push_back (std::make_unique<units::LoFiUnit>());
            break;
    }
    jassert ((int) u.size() == numEffects (c));
    return u;
}

} // namespace kaleido
