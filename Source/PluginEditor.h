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
#include "PluginProcessor.h"
#include "ui/KaleidoLookAndFeel.h"
#include "ui/LevelMeter.h"
#include "ui/ChainStrip.h"
#include "ui/SlotPanel.h"

/**
    Milestone-1 editor, deliberately plain:

      header : wordmark, input LEVEL + TRIM, DRIFT (with an activity light),
               OUTPUT, MIX, meters
      strip  : the four slots as draggable tiles, in chain order
      cards  : one card per slot, laid out in the same order as the strip
*/
class KaleidoAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KaleidoAudioProcessorEditor (KaleidoAudioProcessor&);
    ~KaleidoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutSlots();

    struct LabelledKnob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void setUpKnob (LabelledKnob&, const juce::String& paramId, const juce::String& text,
                    juce::Colour accent);

    KaleidoAudioProcessor& kaleidoProcessor;
    KaleidoLookAndFeel lnf;

    juce::ComboBox inputLevelBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputLevelAttachment;
    LabelledKnob trim, drift, output, mix;
    juce::ToggleButton bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    ChainStrip chain;
    std::array<std::unique_ptr<SlotPanel>, (size_t) kaleido::kNumSlots> panels;
    kaleido::ChainOrder laidOutOrder {};
    juce::Rectangle<int> slotArea;

    juce::Rectangle<int> driftLight;
    float driftGlitchShown = 0.0f, driftWanderShown = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KaleidoAudioProcessorEditor)
};
