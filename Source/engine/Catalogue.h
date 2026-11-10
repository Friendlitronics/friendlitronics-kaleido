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
#include <array>

namespace kaleido
{

/**
    What exists: four categories, each with its own effects, and the labels and
    defaults the UI and the parameter tree need. This is the one place to edit
    when an effect is added or a category is renamed.

    Category names are working names (agreed for v1: Shape / Motion / Space /
    Grain). The parameter ids use the lowercase tags below, so renaming a
    category's *display* name is free, but changing a tag breaks saved sessions.
*/
enum class Category { Shape = 0, Motion, Space, Grain };

inline constexpr int kNumSlots = 4;
inline constexpr int kNumKnobs = 4;            // Amount, Tone, Mix, Extra

enum Knob { kAmount = 0, kTone, kMix, kExtra };

struct EffectInfo
{
    const char* name;                            // shown in the effect menu
    const char* shortName;                       // shown on the chain strip tile
    std::array<const char*, kNumKnobs> labels;   // Amount / Tone / Mix / Extra, as this effect means them
};

struct CategoryInfo
{
    const char* tag;                             // parameter id prefix: never change
    const char* name;                            // display name
    juce::uint32 colour;                         // accent, ARGB
    std::array<float, kNumKnobs> knobDefaults;   // 0..10, shared by the category's effects
    int defaultEffect;
    bool defaultOn;
};

/** Effect order inside a category is the order of the choice parameter, so
    new effects go at the END of a list or saved sessions change effect. */
inline const std::vector<EffectInfo>& effectsFor (Category c)
{
    static const std::vector<EffectInfo> shape {
        { "Screamer",  "SCRM", { "DRIVE",   "TONE",   "MIX", "LEVEL" } },
        { "Fuzz",      "FUZZ", { "SUSTAIN", "TONE",   "MIX", "LEVEL" } },
        { "Sustainer", "SUST", { "SUSTAIN", "ATTACK", "MIX", "LEVEL" } },
    };
    static const std::vector<EffectInfo> motion {
        { "Phaser",    "PHAS", { "DEPTH",   "FDBK",   "MIX", "RATE"  } },
        { "Chorus",    "CHOR", { "DEPTH",   "TONE",   "MIX", "RATE"  } },
        { "Dive",      "DIVE", { "PITCH",   "DOUBLE", "MIX", "GLIDE" } },
    };
    static const std::vector<EffectInfo> space {
        { "Delay",     "DLY",  { "REPEATS", "TONE",   "MIX", "TIME"  } },
        { "Spring",    "SPRG", { "DECAY",   "TONE",   "MIX", "WIDTH" } },
        { "Room",      "ROOM", { "SIZE",    "TONE",   "MIX", "WIDTH" } },
    };
    static const std::vector<EffectInfo> grain {
        { "Tape",      "TAPE", { "SATURATE", "TONE",  "MIX", "FLUTTER" } },
        { "LoFi",      "LOFI", { "CRUSH",   "DECIMATE", "MIX", "NOISE" } },
    };

    switch (c)
    {
        case Category::Shape:  return shape;
        case Category::Motion: return motion;
        case Category::Space:  return space;
        case Category::Grain:  return grain;
    }
    return shape;
}

inline const CategoryInfo& categoryInfo (Category c)
{
    // Knob defaults: Amount, Tone, Mix, Extra (0..10). Motion's Amount sits at
    // 5 because that is unison on Dive.
    static const std::array<CategoryInfo, kNumSlots> infos { {
        { "shape",  "Shape",  0xffff9f43, { 4.0f, 5.0f, 10.0f, 5.0f }, 0, false },
        { "motion", "Motion", 0xff2ed3b7, { 5.0f, 5.0f, 10.0f, 3.0f }, 1, true  },
        { "space",  "Space",  0xff8f7cff, { 3.0f, 5.0f,  4.0f, 3.0f }, 0, true  },
        { "grain",  "Grain",  0xffff5d8f, { 4.0f, 5.0f, 10.0f, 2.0f }, 0, false },
    } };
    return infos[(size_t) c];
}

inline int numEffects (Category c) { return (int) effectsFor (c).size(); }

inline const EffectInfo& effectInfo (Category c, int index)
{
    const auto& list = effectsFor (c);
    return list[(size_t) juce::jlimit (0, (int) list.size() - 1, index)];
}

//==============================================================================
// Parameter ids. Built from string literals: juce::String ('A') would be the
// numeric constructor (the amp's chain strip learned that the hard way).
inline juce::String slotParamId (Category c, const char* suffix)
{
    return juce::String (categoryInfo (c).tag) + suffix;
}

inline juce::String onParamId (Category c)     { return slotParamId (c, "On"); }
inline juce::String effectParamId (Category c) { return slotParamId (c, "Fx"); }

inline juce::String knobParamId (Category c, int knob)
{
    static const char* const suffix[] = { "Amount", "Tone", "Mix", "Extra" };
    return slotParamId (c, suffix[juce::jlimit (0, kNumKnobs - 1, knob)]);
}

//==============================================================================
/** Chain order: which category runs at each position. Every category appears
    exactly once; this is validated wherever an order comes from outside. */
using ChainOrder = std::array<int, (size_t) kNumSlots>;

inline ChainOrder defaultChainOrder() { return { 0, 1, 2, 3 }; }

inline bool chainOrderValid (const ChainOrder& order)
{
    std::array<bool, (size_t) kNumSlots> seen {};
    for (int v : order)
    {
        if (v < 0 || v >= kNumSlots || seen[(size_t) v])
            return false;
        seen[(size_t) v] = true;
    }
    return true;
}

/** Saved as text, e.g. "0,1,2,3". */
inline juce::String chainOrderToString (const ChainOrder& order)
{
    juce::StringArray parts;
    for (int v : order)
        parts.add (juce::String (v));
    return parts.joinIntoString (",");
}

inline ChainOrder chainOrderFromString (const juce::String& text)
{
    juce::StringArray parts;
    parts.addTokens (text, ",", {});
    if (parts.size() != kNumSlots)
        return defaultChainOrder();

    ChainOrder order {};
    for (int i = 0; i < kNumSlots; ++i)
        order[(size_t) i] = parts[i].getIntValue();
    return chainOrderValid (order) ? order : defaultChainOrder();
}

} // namespace kaleido
