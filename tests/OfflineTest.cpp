// Offline checks for both engines: null test, transient/sustain response, level independence, safety clip, CPU.
// Build with -DPOTATO_TESTS=ON and run PotatoShaperTests. Exits non-zero if a check fails.
#include "PluginProcessor.h"
#include <cstdio>

namespace
{
    constexpr double sr = 48000.0;
    constexpr int block = 512, hitEvery = 12000, numHits = 10, len = hitEvery * numHits;

    // Drum-like hits: 120 Hz body (150 ms decay) plus a 10 ms noise click, with a 1 ms attack ramp
    juce::AudioBuffer<float> makeHits (float level)
    {
        juce::AudioBuffer<float> b (2, len);
        juce::Random rnd (1);
        for (int i = 0; i < len; ++i)
        {
            const double t = (i % hitEvery) / sr;
            const double ramp = juce::jmin (1.0, t / 0.001);
            const double v = ramp * (std::sin (juce::MathConstants<double>::twoPi * 120.0 * t) * std::exp (-t / 0.15)
                                     + 0.6 * (rnd.nextFloat() * 2.0f - 1.0f) * std::exp (-t / 0.01));
            b.setSample (0, i, (float) (v * level));
            b.setSample (1, i, (float) (v * level * 0.9));
        }
        return b;
    }

    struct Result { double onsetDb = 0, tailDb = 0, nullDb = 0, peak = 0, cpu = 0; bool finite = true; };

    Result run (int mode, std::initializer_list<std::pair<const char*, float>> settings, float level)
    {
        PotatoShaperProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        auto set = [&] (const char* pid, float v) { auto* prm = p.apvts.getParameter (pid); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        set (ps::id::mode, (float) mode);
        for (auto& s : settings) set (s.first, s.second);
        p.prepareToPlay (sr, block);

        auto in = makeHits (level);
        juce::AudioBuffer<float> out (in);
        juce::MidiBuffer midi;
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int s = 0; s < len; s += block)
        {
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, s, juce::jmin (block, len - s));
            p.processBlock (view, midi);
        }
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);

        Result r;
        r.cpu = 100.0 * secs / (len / sr);
        const int lat = p.getLatencySamples();
        double onIn = 0, onOut = 0, tailIn = 0, tailOut = 0, err = 0, ref = 0;
        for (int i = 0; i < len; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                const float y = out.getSample (ch, i);
                r.finite = r.finite && std::isfinite (y);
                r.peak = juce::jmax (r.peak, (double) std::abs (y));
            }
        for (int h = 3; h < numHits - 1; ++h)
        {
            const int on = h * hitEvery;
            for (int i = on; i < on + (int) (0.008 * sr); ++i)
            {
                onIn = juce::jmax (onIn, (double) std::abs (in.getSample (0, i)));
                onOut = juce::jmax (onOut, (double) std::abs (out.getSample (0, i + lat)));
            }
            for (int i = on + (int) (0.08 * sr); i < on + (int) (0.2 * sr); ++i)
            {
                tailIn += juce::square ((double) in.getSample (0, i));
                tailOut += juce::square ((double) out.getSample (0, i + lat));
            }
            for (int i = on; i < on + hitEvery; ++i)
            {
                err += juce::square ((double) out.getSample (0, i + lat) - in.getSample (0, i));
                ref += juce::square ((double) in.getSample (0, i));
            }
        }
        r.onsetDb = 20.0 * std::log10 (onOut / onIn);
        r.tailDb = 10.0 * std::log10 (tailOut / tailIn);
        r.nullDb = 10.0 * std::log10 (err / ref + 1e-30);
        return r;
    }

    // Distortion on a steady 100 Hz tone with heavy settings: the gain must not ripple with the waveform
    double steadyToneResidualDb (int mode)
    {
        PotatoShaperProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        auto set = [&] (const char* pid, float v) { auto* prm = p.apvts.getParameter (pid); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        set (ps::id::mode, (float) mode); set (ps::id::trans, 12.0f); set (ps::id::sus, 12.0f);
        p.prepareToPlay (sr, block);
        const int n = (int) sr * 3;
        juce::AudioBuffer<float> in (2, n);
        for (int i = 0; i < n; ++i)
            for (int ch = 0; ch < 2; ++ch) in.setSample (ch, i, 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * 100.0 * i / sr));
        juce::AudioBuffer<float> out (in);
        juce::MidiBuffer midi;
        for (int s = 0; s < n; s += block)
        {
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, s, juce::jmin (block, n - s));
            p.processBlock (view, midi);
        }
        const int lat = p.getLatencySamples();
        double xy = 0, xx = 0;
        for (int i = n / 2; i < n - lat; ++i) { xy += (double) in.getSample (0, i) * out.getSample (0, i + lat); xx += juce::square ((double) in.getSample (0, i)); }
        const double g = xy / xx;
        double e = 0;
        for (int i = n / 2; i < n - lat; ++i) e += juce::square (out.getSample (0, i + lat) - g * in.getSample (0, i));
        return 10.0 * std::log10 (e / (g * g * xx) + 1e-30);
    }

    int failures = 0;
    void check (bool ok, const char* what)
    {
        std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what);
        if (! ok) ++failures;
    }
}

// Renders the editor with live audio running into a PNG (used for GUI previews): PotatoShaperTests --snapshot file.png [spectral]
int snapshot (const juce::File& file, bool spectral)
{
    PotatoShaperProcessor p;
    p.setPlayConfigDetails (2, 2, sr, block);
    auto set = [&] (const char* pid, float v) { auto* prm = p.apvts.getParameter (pid); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
    set (ps::id::mode, spectral ? 1.0f : 0.0f);
    set (ps::id::trans, 6.5f); set (ps::id::sus, -3.0f); set (ps::id::time, 18.0f); set (ps::id::warmth, 30.0f);
    set (ps::id::loCut, 30.0f); set (ps::id::hiCut, 14000.0f); set (ps::id::lowG, 5.0f); set (ps::id::midG, -4.0f); set (ps::id::highG, 4.0f);
    set (ps::id::makeup, 1.0f);
    p.prepareToPlay (sr, block);

    auto hits = makeHits (0.3f);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1200, 760);
    juce::MidiBuffer midi;
    int pos = 0;
    for (int frame = 0; frame < 75; ++frame)
    {
        for (int b = 0; b < 3; ++b)
        {
            juce::AudioBuffer<float> buf (2, block);
            for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, hits, ch, pos, block);
            pos = (pos + block) % (len - block);
            p.processBlock (buf, midi);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (34);
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    return png.writeImageToStream (img, out) ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
        return snapshot (juce::File (argv[2]), argc >= 4 && juce::String (argv[3]) == "spectral");
    for (int mode : { 0, 1 })
    {
        std::printf ("\n== %s engine ==\n", mode == 0 ? "CLEAN" : "SPECTRAL");
        const auto neutral = run (mode, {}, 0.25f);
        const auto tUp     = run (mode, { { ps::id::trans, 12.0f } }, 0.1f);
        const auto tUpLow  = run (mode, { { ps::id::trans, 12.0f } }, 0.01f);
        const auto tDown   = run (mode, { { ps::id::trans, -12.0f } }, 0.25f);
        const auto sUp     = run (mode, { { ps::id::sus, 12.0f } }, 0.25f);
        const auto sDown   = run (mode, { { ps::id::sus, -12.0f } }, 0.25f);
        const auto hot     = run (mode, { { ps::id::trans, 24.0f }, { ps::id::makeup, 12.0f }, { ps::id::warmth, 60.0f }, { ps::id::os, 2.0f } }, 0.5f);

        auto row = [] (const char* name, const Result& r)
        { std::printf ("  %-22s onset %+6.2f dB  tail %+6.2f dB  null %7.1f dB  peak %.3f  cpu %.1f%%\n", name, r.onsetDb, r.tailDb, r.nullDb, r.peak, r.cpu); };
        row ("neutral", neutral);
        row ("transient +12", tUp);
        row ("transient +12 (-20dB)", tUpLow);
        row ("transient -12", tDown);
        row ("sustain +12", sUp);
        row ("sustain -12", sDown);
        row ("hot + warmth + 4x OS", hot);

        check (neutral.nullDb < (mode == 0 ? -120.0 : -60.0), "neutral settings pass audio unchanged (latency aligned)");
        check (tUp.onsetDb > 4.0 && std::abs (tUp.tailDb) < 2.0, "transient boost raises the attack, leaves the tail");
        check (tDown.onsetDb < -4.0 && std::abs (tDown.tailDb) < 2.0, "transient cut lowers the attack, leaves the tail");
        check (sUp.tailDb > 4.0 && std::abs (sUp.onsetDb) < 3.0, "sustain boost raises the tail, leaves the attack");
        check (sDown.tailDb < -4.0 && std::abs (sDown.onsetDb) < 3.0, "sustain cut lowers the tail, leaves the attack");
        check (std::abs (tUpLow.onsetDb - tUp.onsetDb) < 0.5 && std::abs (tUpLow.tailDb - tUp.tailDb) < 0.5, "detection is level independent (-20 dB input, same result)");
        const double thd = steadyToneResidualDb (mode);
        std::printf ("  steady 100 Hz tone, transient +12 / sustain +12: residual %.1f dB\n", thd);
        check (thd < -60.0, "steady tone stays clean (no gain ripple distortion)");
        check (hot.peak <= 1.0001 && hot.finite, "safety clip holds the output at 0 dBFS, no NaN/inf");
    }
    std::printf ("\n%s (%d failed)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
