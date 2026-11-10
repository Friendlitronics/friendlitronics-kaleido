/*
    This file is part of Friendlitronics Kaleido — a stereo multi-effects plugin.
    Adapted from the pedal chain strip in Friendlitronics Amp.
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
    The signal chain as four draggable tiles, one per category, left to right in
    processing order. Drag a tile to reorder; click it to switch that slot on
    or off. Each tile shows the category and its current effect.

    Same mechanics as the amp's strip (rotate-on-drag, the dragged tile painted
    last so it floats), minus the amp token.
*/
class ChainStrip : public juce::Component, private juce::Timer
{
public:
    explicit ChainStrip (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        order = kaleido::defaultChainOrder();
        startTimerHz (10);
    }

    std::function<kaleido::ChainOrder()> getOrder;
    std::function<void (const kaleido::ChainOrder&)> setOrder;

    void paint (juce::Graphics& g) override
    {
        g.setColour (KaleidoLookAndFeel::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

        // Flow arrows between the tiles.
        g.setColour (KaleidoLookAndFeel::textDim.withAlpha (0.6f));
        for (int pos = 0; pos < kaleido::kNumSlots - 1; ++pos)
        {
            const auto r = tileBounds (pos);
            const float x = r.getRight() + kGap * 0.5f, y = r.getCentreY();
            juce::Path a;
            a.addTriangle (x - 3.0f, y - 4.0f, x - 3.0f, y + 4.0f, x + 3.0f, y);
            g.fillPath (a);
        }

        for (int pos = 0; pos < kaleido::kNumSlots; ++pos)
            if (pos != dragPos)
                drawTile (g, order[(size_t) pos], tileBounds (pos), false);

        if (dragPos >= 0)
        {
            auto r = tileBounds (dragPos).withX (dragX - tileWidth() * 0.5f);
            drawTile (g, order[(size_t) dragPos], r, true);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragPos = positionAt (e.position.x);
        dragX = e.position.x;
        dragged = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragPos < 0)
            return;

        if (e.getDistanceFromDragStart() > 3)
            dragged = true;
        dragX = e.position.x;

        const int target = positionAt (dragX);
        if (target != dragPos && target >= 0)
        {
            const int item = order[(size_t) dragPos];
            if (target < dragPos)
                std::rotate (order.begin() + target, order.begin() + dragPos, order.begin() + dragPos + 1);
            else
                std::rotate (order.begin() + dragPos, order.begin() + dragPos + 1, order.begin() + target + 1);
            order[(size_t) target] = item;
            dragPos = target;
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragPos >= 0)
        {
            if (dragged)
            {
                if (setOrder != nullptr)
                    setOrder (order);
            }
            else if (auto* p = apvts.getParameter (kaleido::onParamId ((kaleido::Category) order[(size_t) dragPos])))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
                p->endChangeGesture();
            }
        }
        dragPos = -1;
        repaint();
    }

private:
    void timerCallback() override
    {
        if (dragPos >= 0)
            return;
        if (getOrder != nullptr)
            order = getOrder();
        repaint();   // also keeps on/off and effect names current
    }

    float tileWidth() const
    {
        return ((float) getWidth() - 2.0f * kMargin - (float) (kaleido::kNumSlots - 1) * kGap)
               / (float) kaleido::kNumSlots;
    }

    juce::Rectangle<float> tileBounds (int pos) const
    {
        const float w = tileWidth();
        return { kMargin + (w + kGap) * (float) pos, 4.0f, w, (float) getHeight() - 8.0f };
    }

    int positionAt (float x) const
    {
        return juce::jlimit (0, kaleido::kNumSlots - 1,
                             (int) ((x - kMargin) / juce::jmax (1.0f, tileWidth() + kGap)));
    }

    float raw (const juce::String& id) const
    {
        if (auto* v = apvts.getRawParameterValue (id))
            return v->load();
        return 0.0f;
    }

    void drawTile (juce::Graphics& g, int item, juce::Rectangle<float> r, bool dragging)
    {
        const auto cat = (kaleido::Category) item;
        const auto& info = kaleido::categoryInfo (cat);
        const bool on = raw (kaleido::onParamId (cat)) > 0.5f;
        const int fx = (int) std::round (raw (kaleido::effectParamId (cat)));
        const juce::Colour accent (info.colour);

        if (dragging)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r.translated (2.0f, 3.0f), 5.0f);
        }

        g.setColour (on ? accent.withAlpha (0.92f) : KaleidoLookAndFeel::ink.brighter (0.05f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? accent.brighter (0.4f) : accent.withAlpha (0.45f));
        g.drawRoundedRectangle (r, 5.0f, 1.2f);

        auto text = r.reduced (8.0f, 3.0f);
        g.setColour (on ? KaleidoLookAndFeel::ink : accent.withAlpha (0.8f));
        g.setFont (juce::Font (11.5f, juce::Font::bold));
        g.drawText (juce::String (info.name).toUpperCase(), text.removeFromTop (text.getHeight() * 0.5f),
                    juce::Justification::centredBottom);
        g.setFont (juce::Font (10.0f));
        g.setColour (on ? KaleidoLookAndFeel::ink.withAlpha (0.75f) : KaleidoLookAndFeel::textDim);
        g.drawText (kaleido::effectInfo (cat, fx).name, text, juce::Justification::centredTop);
    }

    static constexpr float kMargin = 6.0f;
    static constexpr float kGap = 14.0f;

    juce::AudioProcessorValueTreeState& apvts;
    kaleido::ChainOrder order;
    int dragPos = -1;
    float dragX = 0.0f;
    bool dragged = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainStrip)
};
