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
#include "KaleidoLookAndFeel.h"
#include "../engine/Catalogue.h"

/**
    One category's card: name, on switch, effect menu, and the four knobs. The
    knob labels follow the selected effect (the Tone knob reads RATE on a
    phaser), so they are refreshed on a timer rather than wired to the menu,
    which also catches host automation of the effect choice.
*/
class SlotPanel : public juce::Component, private juce::Timer
{
public:
    SlotPanel (juce::AudioProcessorValueTreeState& state, kaleido::Category c)
        : apvts (state), category (c), accent (kaleido::categoryInfo (c).colour)
    {
        onButton.setButtonText ("ON");
        onButton.setColour (juce::ToggleButton::tickColourId, accent);
        addAndMakeVisible (onButton);
        onAttachment = std::make_unique<ButtonAttachment> (apvts, kaleido::onParamId (c), onButton);

        for (const auto& e : kaleido::effectsFor (c))
            effectBox.addItem (e.name, effectBox.getNumItems() + 1);
        addAndMakeVisible (effectBox);
        effectAttachment = std::make_unique<ComboAttachment> (apvts, kaleido::effectParamId (c), effectBox);

        for (int k = 0; k < kaleido::kNumKnobs; ++k)
        {
            auto& kn = knobs[(size_t) k];
            kn.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            kn.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 13);
            kn.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            kn.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            kn.slider.setNumDecimalPlacesToDisplay (1);
            addAndMakeVisible (kn.slider);
            kn.attachment = std::make_unique<SliderAttachment> (apvts, kaleido::knobParamId (c, k), kn.slider);

            kn.label.setJustificationType (juce::Justification::centred);
            kn.label.setFont (juce::Font (10.0f, juce::Font::bold));
            kn.label.setColour (juce::Label::textColourId, KaleidoLookAndFeel::textDim);
            addAndMakeVisible (kn.label);
        }

        refreshLabels();
        startTimerHz (8);
    }

    kaleido::Category getCategory() const { return category; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f);
        const bool on = onButton.getToggleState();

        g.setColour (KaleidoLookAndFeel::panel);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (on ? accent.withAlpha (0.7f) : KaleidoLookAndFeel::panelEdge);
        g.drawRoundedRectangle (b, 8.0f, on ? 1.5f : 1.0f);

        // Accent bar along the top.
        g.setColour (on ? accent : accent.withAlpha (0.3f));
        g.fillRoundedRectangle (b.removeFromTop (4.0f).reduced (10.0f, 0.0f), 2.0f);

        g.setColour (on ? KaleidoLookAndFeel::text : KaleidoLookAndFeel::textDim);
        g.setFont (juce::Font (17.0f, juce::Font::bold));
        g.drawText (juce::String (kaleido::categoryInfo (category).name).toUpperCase(),
                    getLocalBounds().reduced (14, 0).withTop (12).withHeight (24),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (12, 10);
        b.removeFromTop (2);
        auto header = b.removeFromTop (26);
        onButton.setBounds (header.removeFromRight (44).reduced (0, 3));

        b.removeFromTop (8);
        effectBox.setBounds (b.removeFromTop (26));
        b.removeFromTop (10);

        // 2 x 2 knobs: Amount, Tone / Mix, Extra.
        const int rowH = b.getHeight() / 2;
        for (int row = 0; row < 2; ++row)
        {
            auto r = b.removeFromTop (rowH);
            const int w = r.getWidth() / 2;
            for (int col = 0; col < 2; ++col)
            {
                auto cell = r.removeFromLeft (w).reduced (2, 2).withTrimmedBottom (6);
                auto& kn = knobs[(size_t) (row * 2 + col)];
                kn.label.setBounds (cell.removeFromTop (14));
                kn.slider.setBounds (cell);
            }
        }
    }

private:
    void timerCallback() override
    {
        refreshLabels();
        repaint();
    }

    void refreshLabels()
    {
        const int fx = juce::jmax (0, effectBox.getSelectedItemIndex());
        if (fx == shownEffect)
            return;
        shownEffect = fx;
        const auto& info = kaleido::effectInfo (category, fx);
        for (int k = 0; k < kaleido::kNumKnobs; ++k)
            knobs[(size_t) k].label.setText (info.labels[(size_t) k], juce::dontSendNotification);
    }

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct KnobUi
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    juce::AudioProcessorValueTreeState& apvts;
    kaleido::Category category;
    juce::Colour accent;

    juce::ToggleButton onButton;
    juce::ComboBox effectBox;
    std::array<KnobUi, (size_t) kaleido::kNumKnobs> knobs;
    std::unique_ptr<ButtonAttachment> onAttachment;
    std::unique_ptr<ComboAttachment> effectAttachment;
    int shownEffect = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotPanel)
};
