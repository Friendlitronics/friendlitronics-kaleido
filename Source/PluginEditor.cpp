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

#include "PluginEditor.h"

namespace
{
    constexpr int kWidth = 980;
    constexpr int kHeight = 600;
    constexpr int kHeaderH = 112;
    constexpr int kStripH = 50;
    constexpr int kFooterH = 20;
}

KaleidoAudioProcessorEditor::KaleidoAudioProcessorEditor (KaleidoAudioProcessor& p)
    : AudioProcessorEditor (&p), kaleidoProcessor (p), chain (p.getValueTreeState())
{
    setLookAndFeel (&lnf);
    auto& apvts = p.getValueTreeState();

    inputLevelBox.addItemList (KaleidoAudioProcessor::inputLevelNames(), 1);
    addAndMakeVisible (inputLevelBox);
    inputLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, "inputLevel", inputLevelBox);

    const auto neutral = KaleidoLookAndFeel::textDim.brighter (0.3f);
    setUpKnob (trim,   "inputTrim", "TRIM",   neutral);
    setUpKnob (drift,  "drift",     "DRIFT",  KaleidoLookAndFeel::drift);
    setUpKnob (output, "output",    "OUTPUT", neutral);
    setUpKnob (mix,    "mix",       "MIX",    neutral);
    drift.label.setFont (juce::Font (12.0f, juce::Font::bold));
    drift.label.setColour (juce::Label::textColourId, KaleidoLookAndFeel::drift);

    bypassButton.setButtonText ("BYPASS");
    bypassButton.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff6b5b));
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, "bypass", bypassButton);

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    chain.getOrder = [this] { return kaleidoProcessor.getChainOrder(); };
    chain.setOrder = [this] (const kaleido::ChainOrder& o) { kaleidoProcessor.setChainOrder (o); };
    addAndMakeVisible (chain);

    for (int c = 0; c < kaleido::kNumSlots; ++c)
    {
        panels[(size_t) c] = std::make_unique<SlotPanel> (apvts, (kaleido::Category) c);
        addAndMakeVisible (*panels[(size_t) c]);
    }

    setSize (kWidth, kHeight);
    startTimerHz (30);
}

KaleidoAudioProcessorEditor::~KaleidoAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void KaleidoAudioProcessorEditor::setUpKnob (LabelledKnob& k, const juce::String& paramId,
                                             const juce::String& text, juce::Colour accent)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 13);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        kaleidoProcessor.getValueTreeState(), paramId, k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (10.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, KaleidoLookAndFeel::textDim);
    addAndMakeVisible (k.label);
}

void KaleidoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (KaleidoLookAndFeel::ink);

    auto header = getLocalBounds().removeFromTop (kHeaderH).reduced (16, 14);
    auto word = header.removeFromLeft (220);
    g.setColour (KaleidoLookAndFeel::textDim);
    g.setFont (juce::Font (10.5f, juce::Font::bold));
    g.drawText ("FRIENDLITRONICS", word.removeFromTop (30).withTrimmedTop (14), juce::Justification::bottomLeft);

    // Wordmark: each letter takes one of the category colours in turn.
    const juce::String name ("KALEIDO");
    g.setFont (juce::Font (34.0f, juce::Font::bold));
    float x = (float) word.getX();
    for (int i = 0; i < name.length(); ++i)
    {
        const juce::String ch = name.substring (i, i + 1);
        g.setColour (juce::Colour (kaleido::categoryInfo ((kaleido::Category) (i % kaleido::kNumSlots)).colour));
        g.drawText (ch, juce::Rectangle<float> (x, (float) word.getY(), 40.0f, 40.0f),
                    juce::Justification::centredLeft);
        x += g.getCurrentFont().getStringWidthFloat (ch) + 1.0f;
    }

    g.setColour (KaleidoLookAndFeel::panelEdge);
    g.drawHorizontalLine (kHeaderH - 2, 16.0f, (float) getWidth() - 16.0f);

    // Drift activity light: brightness follows the glitch envelope, and a
    // faint halo follows the wander.
    if (! driftLight.isEmpty())
    {
        const auto c = KaleidoLookAndFeel::drift;
        const auto r = driftLight.toFloat();
        g.setColour (c.withAlpha (0.12f + 0.25f * std::abs (driftWanderShown)));
        g.fillEllipse (r.expanded (3.0f));
        g.setColour (c.withAlpha (0.25f + 0.75f * driftGlitchShown));
        g.fillEllipse (r);
    }

    g.setColour (KaleidoLookAndFeel::textDim.withAlpha (0.7f));
    g.setFont (juce::Font (10.5f));
    g.drawText ("Drag the tiles to reorder the chain; click a tile to switch that slot. "
                "DRIFT feeds one shared instability source to every slot.",
                getLocalBounds().removeFromBottom (kFooterH).reduced (18, 2),
                juce::Justification::centredLeft);
}

void KaleidoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (kHeaderH).reduced (16, 12);
    header.removeFromLeft (230);                               // wordmark

    auto placeKnob = [] (LabelledKnob& k, juce::Rectangle<int> r)
    {
        k.label.setBounds (r.removeFromTop (14));
        k.slider.setBounds (r);
    };

    // Input group.
    auto in = header.removeFromLeft (220);
    inMeter.setBounds (in.removeFromLeft (26));
    in.removeFromLeft (8);
    auto levelCol = in.removeFromLeft (110);
    levelCol.removeFromTop (14);
    inputLevelBox.setBounds (levelCol.removeFromTop (26));
    in.removeFromLeft (6);
    placeKnob (trim, in);

    // Output group on the right.
    auto out = header.removeFromRight (260);
    outMeter.setBounds (out.removeFromRight (26));
    out.removeFromRight (8);
    auto bypassCol = out.removeFromRight (70);
    bypassButton.setBounds (bypassCol.withSizeKeepingCentre (66, 22).withY (bypassCol.getY() + 22));
    placeKnob (mix, out.removeFromRight (75));
    placeKnob (output, out.removeFromRight (75));

    // Drift in the middle, a little larger.
    auto mid = header.withSizeKeepingCentre (110, header.getHeight());
    placeKnob (drift, mid);
    driftLight = juce::Rectangle<int> (mid.getRight() + 2, mid.getY() + 2, 9, 9);

    area.removeFromBottom (kFooterH);
    chain.setBounds (area.removeFromTop (kStripH).reduced (16, 4));
    area.removeFromTop (4);
    slotArea = area.reduced (12, 4);
    layoutSlots();
}

void KaleidoAudioProcessorEditor::layoutSlots()
{
    laidOutOrder = kaleidoProcessor.getChainOrder();
    auto area = slotArea;
    const int w = area.getWidth() / kaleido::kNumSlots;
    for (int pos = 0; pos < kaleido::kNumSlots; ++pos)
        panels[(size_t) laidOutOrder[(size_t) pos]]->setBounds (area.removeFromLeft (w).reduced (4, 4));
}

void KaleidoAudioProcessorEditor::timerCallback()
{
    inMeter.setLevel (kaleidoProcessor.getInputLevel());
    outMeter.setLevel (kaleidoProcessor.getOutputLevel());

    // Cards follow the strip once a drag is dropped.
    if (kaleidoProcessor.getChainOrder() != laidOutOrder)
        layoutSlots();

    const float g = kaleidoProcessor.getDriftGlitch();
    driftGlitchShown = juce::jmax (g, driftGlitchShown * 0.85f);
    driftWanderShown = kaleidoProcessor.getDriftWander();
    repaint (driftLight.expanded (4));
}
