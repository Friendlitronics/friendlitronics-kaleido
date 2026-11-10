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

#include "KaleidoLookAndFeel.h"

const juce::Colour KaleidoLookAndFeel::ink       = juce::Colour (0xff14121c);
const juce::Colour KaleidoLookAndFeel::panel     = juce::Colour (0xff1f1c2b);
const juce::Colour KaleidoLookAndFeel::panelEdge = juce::Colour (0xff332f45);
const juce::Colour KaleidoLookAndFeel::text      = juce::Colour (0xffe9e4f5);
const juce::Colour KaleidoLookAndFeel::textDim   = juce::Colour (0xff8d86a3);
const juce::Colour KaleidoLookAndFeel::drift     = juce::Colour (0xfff4e285);

KaleidoLookAndFeel::KaleidoLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, ink);

    setColour (juce::Slider::textBoxTextColourId, text.withAlpha (0.8f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, drift);

    setColour (juce::Label::textColourId, text);

    setColour (juce::ComboBox::backgroundColourId, ink.brighter (0.06f));
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, textDim);

    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, panelEdge.brighter (0.2f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);

    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, drift);
}

void KaleidoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider& slider)
{
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float ring = juce::jmax (2.5f, r * 0.12f);

    juce::Path track;
    track.addCentredArc (cx, cy, r - ring * 0.5f, r - ring * 0.5f, 0.0f, startAngle, endAngle, true);
    g.setColour (panelEdge);
    g.strokePath (track, juce::PathStrokeType (ring, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (cx, cy, r - ring * 0.5f, r - ring * 0.5f, 0.0f, startAngle, angle, true);
    g.setColour (slider.isEnabled() ? accent : accent.withSaturation (0.1f).withAlpha (0.4f));
    g.strokePath (value, juce::PathStrokeType (ring, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    const float kr = r - ring - 3.0f;
    g.setColour (panel.brighter (0.12f));
    g.fillEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    juce::Path dot;
    const float dr = juce::jmax (1.8f, kr * 0.14f);
    dot.addEllipse (-dr, -kr * 0.72f - dr, dr * 2.0f, dr * 2.0f);
    dot.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
    g.setColour (text);
    g.fillPath (dot);
}

void KaleidoLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    const float ax = (float) width - 15.0f, ay = (float) height * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (ax, ay - 2.0f);
    arrow.lineTo (ax + 4.0f, ay + 2.0f);
    arrow.lineTo (ax + 8.0f, ay - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

void KaleidoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                           bool highlighted, bool)
{
    // A pill that lights in the button's tick colour when on.
    const bool on = button.getToggleState();
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);
    auto b = button.getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (on ? accent : ink.brighter (highlighted ? 0.12f : 0.06f));
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (on ? accent.brighter (0.3f) : panelEdge);
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);

    g.setColour (on ? ink : textDim);
    g.setFont (juce::Font (10.5f, juce::Font::bold));
    g.drawText (button.getButtonText(), b, juce::Justification::centred);
}
