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
#include <atomic>
#include <array>

#include "engine/KaleidoEngine.h"

/**
    Kaleido processor: owns the parameter tree and feeds kaleido::KaleidoEngine
    each block, plus output gain, a global dry/wet and metering. Same shape as
    the Amp and Tape processors.
*/
class KaleidoAudioProcessor : public juce::AudioProcessor
{
public:
    KaleidoAudioProcessor();
    ~KaleidoAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }   // delay near self-oscillation

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    /** The slot order. Lives in the state tree (so it travels with sessions)
        and is mirrored into an atomic for the audio thread. */
    kaleido::ChainOrder getChainOrder() const;
    void setChainOrder (const kaleido::ChainOrder& order);

    float getInputLevel()  const { return inputLevel.load();  }
    float getOutputLevel() const { return outputLevel.load(); }
    float getDriftWander() const { return driftWander.load(); }
    float getDriftGlitch() const { return driftGlitch.load(); }

    static juce::StringArray inputLevelNames() { return { "Instrument", "Line" }; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void publishChainOrder (const kaleido::ChainOrder&);
    void readChainOrderFromState();

    juce::AudioProcessorValueTreeState apvts;

    struct SlotParams
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* effect = nullptr;
        std::array<std::atomic<float>*, (size_t) kaleido::kNumKnobs> knob {};
    };
    std::array<SlotParams, (size_t) kaleido::kNumSlots> slotParams;

    std::atomic<float>* inputLevelParam = nullptr;
    std::atomic<float>* inputTrimParam  = nullptr;
    std::atomic<float>* driftParam      = nullptr;
    std::atomic<float>* outputParam     = nullptr;
    std::atomic<float>* mixParam        = nullptr;
    std::atomic<float>* bypassParam     = nullptr;

    /** Chain order, 2 bits per position. */
    std::atomic<int> packedChain { 0 };

    kaleido::KaleidoEngine engine;
    juce::dsp::Gain<float> outputGain;
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    std::atomic<float> inputLevel  { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> driftWander { 0.0f };
    std::atomic<float> driftGlitch { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KaleidoAudioProcessor)
};
