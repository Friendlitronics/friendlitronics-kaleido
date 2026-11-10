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

/**
    Placeholder look for milestone 1: a dark ink panel with one accent colour
    per category. Knobs take their accent from Slider::rotarySliderFillColourId,
    so each slot tints its own. Fully programmatic, no image assets. The real
    UI direction is still to be decided (docs/TODO.md).
*/
class KaleidoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static const juce::Colour ink;        // background
    static const juce::Colour panel;      // slot cards
    static const juce::Colour panelEdge;
    static const juce::Colour text;
    static const juce::Colour textDim;
    static const juce::Colour drift;      // the Drift knob's own colour

    KaleidoLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int, int, int, int, juce::ComboBox&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;

    juce::Font getLabelFont (juce::Label& label) override { return label.getFont(); }
};
