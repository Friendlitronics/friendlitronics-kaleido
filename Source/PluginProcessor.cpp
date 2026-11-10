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

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace kaleido;

namespace ParamID
{
    static constexpr auto inputLevel = "inputLevel";
    static constexpr auto inputTrim  = "inputTrim";
    static constexpr auto drift      = "drift";
    static constexpr auto output     = "output";
    static constexpr auto mix        = "mix";
    static constexpr auto bypass     = "bypass";
}

static constexpr const char* kChainProperty = "chainOrder";

KaleidoAudioProcessor::KaleidoAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int c = 0; c < kNumSlots; ++c)
    {
        const auto cat = (Category) c;
        auto& sp = slotParams[(size_t) c];
        sp.on     = apvts.getRawParameterValue (onParamId (cat));
        sp.effect = apvts.getRawParameterValue (effectParamId (cat));
        for (int k = 0; k < kNumKnobs; ++k)
            sp.knob[(size_t) k] = apvts.getRawParameterValue (knobParamId (cat, k));
    }

    inputLevelParam = apvts.getRawParameterValue (ParamID::inputLevel);
    inputTrimParam  = apvts.getRawParameterValue (ParamID::inputTrim);
    driftParam      = apvts.getRawParameterValue (ParamID::drift);
    outputParam     = apvts.getRawParameterValue (ParamID::output);
    mixParam        = apvts.getRawParameterValue (ParamID::mix);
    bypassParam     = apvts.getRawParameterValue (ParamID::bypass);

    publishChainOrder (defaultChainOrder());
}

juce::AudioProcessorValueTreeState::ParameterLayout
KaleidoAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    // One decimal everywhere, and never "-0.0".
    auto oneDp = [] (float v, int) { return String (std::abs (v) < 0.05f ? 0.0f : v, 1); };
    const auto knobAttrs = AudioParameterFloatAttributes().withStringFromValueFunction (oneDp);
    const auto dbAttrs   = AudioParameterFloatAttributes().withStringFromValueFunction (oneDp).withLabel ("dB");

    for (int c = 0; c < kNumSlots; ++c)
    {
        const auto cat = (Category) c;
        const auto& info = categoryInfo (cat);
        const String name (info.name);

        StringArray fxNames;
        for (const auto& e : effectsFor (cat))
            fxNames.add (e.name);

        auto group = std::make_unique<AudioProcessorParameterGroup> (info.tag, name, " | ");
        group->addChild (std::make_unique<AudioParameterBool> (
            ParameterID { onParamId (cat), 1 }, name + " On", info.defaultOn));
        group->addChild (std::make_unique<AudioParameterChoice> (
            ParameterID { effectParamId (cat), 1 }, name + " Effect", fxNames, info.defaultEffect));

        static const char* const knobNames[] = { "Amount", "Tone", "Mix", "Extra" };
        for (int k = 0; k < kNumKnobs; ++k)
            group->addChild (std::make_unique<AudioParameterFloat> (
                ParameterID { knobParamId (cat, k), 1 }, name + " " + knobNames[k],
                NormalisableRange<float> (0.0f, 10.0f, 0.01f), info.knobDefaults[(size_t) k], knobAttrs));

        layout.add (std::move (group));
    }

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::inputLevel, 1 }, "Input Level", inputLevelNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::inputTrim, 1 }, "Input Trim",
        NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f, dbAttrs));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::drift, 1 }, "Drift",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 0.0f, knobAttrs));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f, dbAttrs));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 0); })
                                       .withLabel ("%")));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::bypass, 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
void KaleidoAudioProcessor::publishChainOrder (const ChainOrder& order)
{
    int packed = 0;
    for (int i = 0; i < kNumSlots; ++i)
        packed |= (order[(size_t) i] & 0x3) << (2 * i);
    packedChain.store (packed);
}

ChainOrder KaleidoAudioProcessor::getChainOrder() const
{
    const int packed = packedChain.load();
    ChainOrder order {};
    for (int i = 0; i < kNumSlots; ++i)
        order[(size_t) i] = (packed >> (2 * i)) & 0x3;
    return chainOrderValid (order) ? order : defaultChainOrder();
}

void KaleidoAudioProcessor::setChainOrder (const ChainOrder& order)
{
    if (! chainOrderValid (order))
        return;
    publishChainOrder (order);
    apvts.state.setProperty (kChainProperty, chainOrderToString (order), nullptr);
}

void KaleidoAudioProcessor::readChainOrderFromState()
{
    const auto text = apvts.state.getProperty (kChainProperty).toString();
    publishChainOrder (text.isNotEmpty() ? chainOrderFromString (text) : defaultChainOrder());
}

//==============================================================================
void KaleidoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());

    engine.prepare (sampleRate, samplesPerBlock, numChannels);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numChannels };
    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);

    dryBuffer.setSize (numChannels, samplesPerBlock);
    dryBuffer.clear();

    mixSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() * 0.01f);

    inputLevel.store (0.0f);
    outputLevel.store (0.0f);
    setLatencySamples (0);
}

void KaleidoAudioProcessor::releaseResources()
{
    engine.reset();
}

bool KaleidoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    const bool monoOrStereoOut = out == juce::AudioChannelSet::mono()
                              || out == juce::AudioChannelSet::stereo();
    if (! monoOrStereoOut)
        return false;

    // Same in and out, or mono in -> stereo out (a guitar into a stereo chain).
    return in == out
        || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void KaleidoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Mono in, stereo out: both sides start from the one input.
    if (totalIn == 1 && totalOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    else
        for (int ch = totalIn; ch < totalOut; ++ch)
            buffer.clear (ch, 0, numSamples);

    if (*bypassParam > 0.5f)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < totalOut; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));
        inputLevel.store (peak);
        outputLevel.store (peak);
        return;
    }

    const int chans = juce::jmin (totalOut, dryBuffer.getNumChannels());
    for (int ch = 0; ch < chans; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    KaleidoEngine::Params p;
    for (int c = 0; c < kNumSlots; ++c)
    {
        const auto& sp = slotParams[(size_t) c];
        auto& out = p.slots[(size_t) c];
        out.on = sp.on->load() > 0.5f;
        out.effect = (int) std::round (sp.effect->load());
        out.knobs.amount = sp.knob[kAmount]->load() * 0.1f;
        out.knobs.tone   = sp.knob[kTone]->load()   * 0.1f;
        out.knobs.mix    = sp.knob[kMix]->load()    * 0.1f;
        out.knobs.extra  = sp.knob[kExtra]->load()  * 0.1f;
    }
    p.order = getChainOrder();
    p.drift = driftParam->load() * 0.1f;
    p.inputLevel = (int) std::round (inputLevelParam->load()) == 1 ? InputStage::Level::Line
                                                                    : InputStage::Level::Instrument;
    p.inputTrimDb = inputTrimParam->load();
    engine.setParams (p);

    juce::dsp::AudioBlock<float> block (buffer);
    auto sub = block.getSubsetChannelBlock (0, (size_t) chans);
    engine.process (sub);
    inputLevel.store (engine.lastChainInputPeak());

    outputGain.setGainDecibels (outputParam->load());
    juce::dsp::ProcessContextReplacing<float> ctx (sub);
    outputGain.process (ctx);

    // Global dry/wet, for using Kaleido on a send-less bus. No latency, so no
    // dry delay is needed.
    mixSmoothed.setTargetValue (mixParam->load() * 0.01f);
    for (int i = 0; i < numSamples; ++i)
    {
        const float m = mixSmoothed.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            wet[i] = wet[i] * m + dryBuffer.getSample (ch, i) * (1.0f - m);
        }
    }

    float outPeak = 0.0f;
    for (int ch = 0; ch < totalOut; ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, numSamples));
    outputLevel.store (outPeak);

    const auto& d = engine.driftState();
    driftWander.store (d.depth * d.wander);
    driftGlitch.store (d.depth * d.glitch);
}

//==============================================================================
juce::AudioProcessorEditor* KaleidoAudioProcessor::createEditor()
{
    return new KaleidoAudioProcessorEditor (*this);
}

void KaleidoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void KaleidoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid() && tree.hasType (apvts.state.getType()))
    {
        apvts.replaceState (tree);
        readChainOrderFromState();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KaleidoAudioProcessor();
}
