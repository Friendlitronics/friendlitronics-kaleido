/*
    Headless engine test for Friendlitronics Kaleido.

    Build:  cmake --build build --target KaleidoEngineTest
    Run:    the binary prints one line per check and exits non-zero on failure.

    What it checks:
      1. Every effect in every slot, alone, at default and extreme knob
         settings: output stays finite and bounded, and is not silent.
      2. All slots off: the chain is transparent apart from the 18 Hz input
         high-pass (checked against a 1 kHz sine).
      3. Line mode: the pad and its undo cancel, so a bypassed chain is unity.
      4. Reordering mid-stream: the order actually changes, and the duck does
         not leave a gap longer than ~15 ms.
      5. Drift at full on every slot at once: finite, bounded, and the glitch
         events actually fire.
      6. The chain order string round-trips and rejects bad input.
*/

#include <JuceHeader.h>
#include <cstdio>
#include "../Source/engine/KaleidoEngine.h"

using namespace kaleido;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }

    /** A guitar-ish test signal: a plucked 110 Hz saw with harmonics, decaying
        and re-plucked every 0.5 s, peaking around -10 dBFS. */
    struct Source
    {
        float next()
        {
            const float env = std::exp (-3.0f * (float) (n % 24000) / 24000.0f);
            phase += 110.0f / (float) kSr;
            if (phase >= 1.0f) phase -= 1.0f;
            ++n;
            return 0.3f * env * (2.0f * phase - 1.0f);
        }
        float phase = 0.0f;
        long n = 0;
    };

    struct Stats { float peak = 0.0f; double energy = 0.0; bool finite = true; };

    Stats run (KaleidoEngine& e, int blocks, Source& src)
    {
        Stats s;
        juce::AudioBuffer<float> buf (2, kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < kBlock; ++i)
            {
                const float x = src.next();
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
            }
            juce::dsp::AudioBlock<float> block (buf);
            e.process (block);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                {
                    const float y = buf.getSample (ch, i);
                    if (! std::isfinite (y)) s.finite = false;
                    s.peak = juce::jmax (s.peak, std::abs (y));
                    s.energy += (double) y * y;
                }
        }
        return s;
    }

    KaleidoEngine::Params allOff()
    {
        KaleidoEngine::Params p;
        for (auto& s : p.slots) s.on = false;
        return p;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;   // juce::dsp::Reverb and friends want JUCE up

    // --- 1. every effect, alone ---
    for (int c = 0; c < kNumSlots; ++c)
    {
        const auto cat = (Category) c;
        for (int fx = 0; fx < numEffects (cat); ++fx)
        {
            for (float setting : { -1.0f, 0.0f, 1.0f })   // -1 = category defaults
            {
                KaleidoEngine e;
                e.prepare (kSr, kBlock, 2);
                auto p = allOff();
                auto& s = p.slots[(size_t) c];
                s.on = true;
                s.effect = fx;
                const auto& d = categoryInfo (cat).knobDefaults;
                s.knobs = { d[0] * 0.1f, d[1] * 0.1f, 1.0f, d[3] * 0.1f };
                if (setting >= 0.0f)
                    s.knobs = { setting, setting, 1.0f, setting };
                e.setParams (p);

                Source src;
                const auto st = run (e, 400, src);   // ~2 s
                const juce::String label = juce::String (categoryInfo (cat).name) + " / "
                                         + effectInfo (cat, fx).name
                                         + (setting < 0.0f ? " defaults" : setting == 0.0f ? " knobs 0" : " knobs 10");
                check (st.finite, label + ": finite");
                check (st.peak < 8.0f, label + ": bounded (peak " + juce::String (st.peak, 2) + ")");
                check (st.energy > 1.0e-3, label + ": not silent");
            }
        }
    }

    // --- 2. all off is transparent at 1 kHz ---
    {
        KaleidoEngine e;
        e.prepare (kSr, kBlock, 2);
        e.setParams (allOff());
        juce::AudioBuffer<float> buf (2, kBlock);
        float maxErr = 0.0f;
        double ph = 0.0;
        for (int b = 0; b < 200; ++b)
        {
            juce::AudioBuffer<float> ref (2, kBlock);
            for (int i = 0; i < kBlock; ++i)
            {
                const float x = 0.25f * (float) std::sin (ph);
                ph += juce::MathConstants<double>::twoPi * 1000.0 / kSr;
                for (int ch = 0; ch < 2; ++ch) { buf.setSample (ch, i, x); ref.setSample (ch, i, x); }
            }
            juce::dsp::AudioBlock<float> block (buf);
            e.process (block);
            if (b > 20)
                for (int i = 0; i < kBlock; ++i)
                    maxErr = juce::jmax (maxErr, std::abs (buf.getSample (0, i) - ref.getSample (0, i)));
        }
        check (maxErr < 0.01f, "all slots off: transparent at 1 kHz (max err " + juce::String (maxErr, 5) + ")");
    }

    // --- 3. Line mode is unity through an empty chain ---
    {
        KaleidoEngine e;
        e.prepare (kSr, kBlock, 2);
        auto p = allOff();
        p.inputLevel = InputStage::Level::Line;
        e.setParams (p);
        Source warm; run (e, 50, warm);              // let the gain smoothing settle
        Source a, b;
        KaleidoEngine ref;
        ref.prepare (kSr, kBlock, 2);
        ref.setParams (allOff());
        Source warmRef; run (ref, 50, warmRef);
        const auto line = run (e, 100, a);
        const auto inst = run (ref, 100, b);
        const double ratioDb = 10.0 * std::log10 (line.energy / inst.energy);
        check (std::abs (ratioDb) < 0.1, "line pad and undo cancel (" + juce::String (ratioDb, 3) + " dB)");
    }

    // --- 4. reorder mid-stream ---
    {
        KaleidoEngine e;
        e.prepare (kSr, kBlock, 2);
        auto p = allOff();
        p.slots[0].on = true;  p.slots[0].knobs = { 0.6f, 0.5f, 1.0f, 0.5f };   // Screamer
        p.slots[2].on = true;  p.slots[2].knobs = { 0.4f, 0.5f, 0.5f, 0.3f };   // Delay
        e.setParams (p);
        Source src;
        run (e, 100, src);

        p.order = { 3, 2, 1, 0 };
        e.setParams (p);

        // Count the longest run of near-silent samples across the change.
        juce::AudioBuffer<float> buf (2, kBlock);
        int longestQuiet = 0, quiet = 0;
        for (int b = 0; b < 40; ++b)
        {
            for (int i = 0; i < kBlock; ++i)
            {
                // Steady 220 Hz tone (no decay), so "quiet" can only mean the duck.
                const float x = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * (double) (b * kBlock + i) / kSr);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            juce::dsp::AudioBlock<float> block (buf);
            e.process (block);
            for (int i = 0; i < kBlock; ++i)
            {
                quiet = std::abs (buf.getSample (0, i)) < 1.0e-4f ? quiet + 1 : 0;
                longestQuiet = juce::jmax (longestQuiet, quiet);
            }
        }
        check (e.currentOrder() == ChainOrder { 3, 2, 1, 0 }, "reorder applied");
        check (longestQuiet < (int) (0.015 * kSr),
               "reorder duck is short (" + juce::String (longestQuiet) + " samples)");
    }

    // --- 5. full Drift on everything ---
    {
        KaleidoEngine e;
        e.prepare (kSr, kBlock, 2);
        KaleidoEngine::Params p;
        for (int c = 0; c < kNumSlots; ++c)
        {
            p.slots[(size_t) c].on = true;
            const auto& d = categoryInfo ((Category) c).knobDefaults;
            p.slots[(size_t) c].knobs = { d[0] * 0.1f, d[1] * 0.1f, d[2] * 0.1f, d[3] * 0.1f };
        }
        p.drift = 1.0f;
        e.setParams (p);

        Source src;
        int glitches = 0;
        bool wasGlitching = false;
        Stats total;
        for (int chunk = 0; chunk < 40; ++chunk)                 // 40 x 0.5 s = 20 s
        {
            const auto st = run (e, 94, src);
            total.finite = total.finite && st.finite;
            total.peak = juce::jmax (total.peak, st.peak);
            const bool g = e.driftState().glitch > 0.0f;
            if (g && ! wasGlitching) ++glitches;
            wasGlitching = g;
        }
        check (total.finite, "full drift: finite");
        check (total.peak < 8.0f, "full drift: bounded (peak " + juce::String (total.peak, 2) + ")");
        check (glitches > 0, "full drift: glitches fire (" + juce::String (glitches) + " seen at 0.5 s sampling)");
    }

    // --- 6. chain order text ---
    check (chainOrderFromString ("3,1,0,2") == ChainOrder { 3, 1, 0, 2 }, "order round-trips");
    check (chainOrderFromString ("0,0,1,2") == defaultChainOrder(), "duplicate order rejected");
    check (chainOrderFromString ("0,1,2") == defaultChainOrder(), "short order rejected");
    check (chainOrderFromString ("0,1,2,7") == defaultChainOrder(), "out-of-range order rejected");

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
