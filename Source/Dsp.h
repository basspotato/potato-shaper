#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <complex>
#include <array>

namespace ps
{
    enum Listen { listenOut, listenTrans, listenSus, listenDelta, listenSc };

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)  { return 20.0f * std::log10 (g + 1.0e-12f); }

    // One-pole coefficient for a time constant in seconds at a given update rate
    inline float onePole (double seconds, double rate) { return (float) std::exp (-1.0 / juce::jmax (1.0e-6, seconds * rate)); }

    //==============================================================================
    // RBJ cookbook biquad, transposed direct form II (double state for low-frequency precision)
    struct BiquadCoeffs
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

        static BiquadCoeffs make (int type, double sr, double f, double q, double gainDb)
        {
            const double w = juce::MathConstants<double>::twoPi * juce::jlimit (5.0, sr * 0.49, f) / sr;
            const double cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0 * q);
            const double A = std::pow (10.0, gainDb / 40.0);
            double b0, b1, b2, a0, a1, a2;
            switch (type)
            {
                case 0: // high-pass
                    b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
                case 1: // low-pass
                    b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
                case 2: // peak
                    b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
                case 3: // low shelf
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s);
                    a0 = (A + 1) + (A - 1) * cw + s; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s; break;
                }
                case 5: // first-order high-pass (bilinear)
                case 6: // first-order low-pass
                {
                    const double k = std::tan (w * 0.5), n = 1.0 / (1.0 + k);
                    return type == 5 ? BiquadCoeffs { n, -n, 0, (k - 1) * n, 0 } : BiquadCoeffs { k * n, k * n, 0, (k - 1) * n, 0 };
                }
                default: // high shelf
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s);
                    a0 = (A + 1) - (A - 1) * cw + s; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s; break;
                }
            }
            return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
        }

        double magnitude (double f, double sr) const
        {
            const std::complex<double> z1 = std::polar (1.0, -juce::MathConstants<double>::twoPi * f / sr), z2 = z1 * z1;
            return std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
        }
    };

    struct Biquad
    {
        double z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        float process (const BiquadCoeffs& c, float xf)
        {
            const double x = xf, y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return (float) y;
        }
    };

    //==============================================================================
    // The Focus curve: lo cut, three bands (each a low shelf, bell, high shelf or tilt shelf) and hi cut. The cuts are
    // Butterworth at 6, 12, 24 or 48 dB/oct; their Q scales the most resonant section. It weights what the detector listens to,
    // and with Focus PROC the two cuts also limit where processing applies. The same coefficients draw the curve in the editor.
    enum BandType { bandLoShelf, bandPeak, bandHiShelf, bandTilt };
    enum CutSlope { slope6, slope12, slope24, slope48 };

    struct FocusSettings
    {
        float loCut = 20, hiCut = 20000, lowF = 90, lowG = 0, lowQ = 0.71f, midF = 800, midG = 0, midQ = 0.71f, highF = 6000, highG = 0, highQ = 0.71f;
        int lowType = bandLoShelf, midType = bandPeak, highType = bandHiShelf;
        float loQ = 0.71f, hiQ = 0.71f;
        int loSlope = slope24, hiSlope = slope24;
    };

    struct FocusCurve
    {
        static constexpr int cutStages = 4, loStage = 0, hiStage = 10;
        static constexpr int numStages = 14;   // lo cut x4, three bands x2 (tilt uses both), hi cut x4
        std::array<BiquadCoeffs, numStages> c;
        std::array<bool, numStages> active {};
        bool loActive = false, hiActive = false;

        void design (const FocusSettings& s, double sr)
        {
            const double nyq = sr * 0.5;
            loActive = s.loCut > 20.5f;
            hiActive = s.hiCut < 19900.0f && s.hiCut < nyq * 0.95;
            designCut (loStage, loActive, true,  s.loSlope, s.loCut, s.loQ, sr);
            designBand (4, s.lowType,  s.lowF,  s.lowG,  s.lowQ,  sr);
            designBand (6, s.midType,  s.midF,  s.midG,  s.midQ,  sr);
            designBand (8, s.highType, s.highF, s.highG, s.highQ, sr);
            designCut (hiStage, hiActive, false, s.hiSlope, s.hiCut, s.hiQ, sr);
        }

        double magnitude (double f, double sr) const
        {
            double m = 1.0;
            for (int i = 0; i < numStages; ++i)
                if (active[(size_t) i]) m *= c[(size_t) i].magnitude (f, sr);
            return m;
        }

        // The two cuts alone: where processing applies when Focus also limits processing
        double bandMagnitude (double f, double sr) const
        {
            double m = 1.0;
            for (int i = 0; i < cutStages; ++i)
            {
                if (active[(size_t) (loStage + i)]) m *= c[(size_t) (loStage + i)].magnitude (f, sr);
                if (active[(size_t) (hiStage + i)]) m *= c[(size_t) (hiStage + i)].magnitude (f, sr);
            }
            return m;
        }
        bool bandActive() const { return loActive || hiActive; }

    private:
        void set (int i, bool on, const BiquadCoeffs& k) { active[(size_t) i] = on; c[(size_t) i] = on ? k : BiquadCoeffs(); }

        // Stages i..i+3: Butterworth sections for the slope; Q (0.71 = flat) scales the most resonant section
        void designCut (int i, bool on, bool highPass, int slope, double f, double q, double sr)
        {
            static constexpr double qs[4][4] { { 0, 0, 0, 0 }, { 0.7071, 0, 0, 0 }, { 0.5412, 1.3066, 0, 0 }, { 0.5098, 0.6013, 0.9000, 2.5629 } };
            static constexpr int sections[4] { 1, 1, 2, 4 };
            slope = juce::jlimit (0, 3, slope);
            for (int k = 0; k < cutStages; ++k)
            {
                if (! on || k >= sections[slope]) { set (i + k, false, {}); continue; }
                if (slope == slope6) { set (i + k, true, BiquadCoeffs::make (highPass ? 5 : 6, sr, f, 0.7071, 0)); continue; }
                double sq = qs[slope][k];
                if (k == sections[slope] - 1) sq = juce::jmin (12.0, sq * q / 0.7071);
                set (i + k, true, BiquadCoeffs::make (highPass ? 0 : 1, sr, f, sq, 0));
            }
        }

        // Stages i and i+1. A band with no gain, or above the usable range, is left out. Tilt = low shelf cut + high shelf boost
        // (each half the gain) around the same frequency, so the curve crosses 0 dB there.
        void designBand (int i, int type, double f, double gDb, double q, double sr)
        {
            const bool on = std::abs (gDb) > 0.005 && f < sr * 0.475;
            if (type == bandTilt)
            {
                set (i, on, BiquadCoeffs::make (3, sr, f, q, -gDb * 0.5));
                set (i + 1, on, BiquadCoeffs::make (4, sr, f, q, gDb * 0.5));
                return;
            }
            set (i, on, BiquadCoeffs::make (type == bandPeak ? 2 : (type == bandHiShelf ? 4 : 3), sr, f, q, gDb));
            set (i + 1, false, {});
        }
    };

    //==============================================================================
    // Analytic-signal generator: two chains of allpass sections whose outputs stay 90 degrees apart (better than 36 dB
    // image rejection from 20 Hz to 20 kHz at 48 kHz; coefficients by Olli Niemitalo). The Spectral engine detects on
    // this signal so a steady tone gives a steady bin magnitude: with a real signal, the tone's mirror image at
    // negative frequency beats against it in low bins and makes the level flicker.
    struct Hilbert
    {
        struct Section { float c = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
        Section a[4], b[4];
        float aDelay = 0;

        Hilbert()
        {
            const float ca[4] { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            const float cb[4] { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            for (int i = 0; i < 4; ++i) { a[i].c = ca[i] * ca[i]; b[i].c = cb[i] * cb[i]; }
        }

        void reset() { for (auto* chain : { a, b }) for (int i = 0; i < 4; ++i) chain[i].x1 = chain[i].x2 = chain[i].y1 = chain[i].y2 = 0; aDelay = 0; }

        static float run (Section* chain, float x)
        {
            for (int i = 0; i < 4; ++i)
            {
                auto& s = chain[i];
                const float y = s.c * (x + s.y2) - s.x2;
                s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
                x = y;
            }
            return x;
        }

        std::complex<float> process (float x)
        {
            const float re = aDelay;
            aDelay = run (a, x);
            return { re, -run (b, x) };   // positive-frequency analytic signal
        }
    };

    //==============================================================================
    // Everything the engines need from the parameters, read once per block
    struct EngineParams
    {
        float transDb = 0, susDb = 0, timeMs = 20, warmth = 0;
        FocusSettings focus;
        bool focusProc = true;
        float link = 1, balance = 0;           // balance -1..1
        bool extSc = false;
        float scGain = 1;
        int listen = 0, window = 1;
        float sharp = 5;
    };

    // How much each channel is processed for a given balance (-1 = only channel 0, +1 = only channel 1)
    inline float balanceAmount (float balance, int ch)
    {
        return ch == 0 ? (balance <= 0.0f ? 1.0f : 1.0f - balance) : (balance >= 0.0f ? 1.0f : 1.0f + balance);
    }

    // Detection maps a level difference in dB to a 0..1 mask. 9 dB of difference counts as fully transient (or sustained).
    inline float diffToMask (float diffDb) { return juce::jlimit (0.0f, 1.0f, diffDb * (1.0f / 9.0f)); }

    // Level-independent differential-envelope detector, shared by both engines. Everything runs on levels in dB,
    // so only the *shape* of the envelope matters, never its absolute level.
    //  - Transient: the level minus a copy of itself smoothed with the Time constant. Positive only while the level
    //    rises faster than Time can follow; it falls back to zero about one Time after the peak.
    //  - Sustain: a long-release hold of the level minus the level. Positive while the sound decays, and suppressed
    //    while the transient is still active, so a longer Time also starts the sustain later.
    struct Detector
    {
        struct Coeffs { float pkRel = 0, lvSm = 0, slow = 0, hold = 0, floorDb = -90; int peakHold = 0; };
        struct Masks  { float m = 0, s = 0; };

        // rate: samples (Clean) or frames (Spectral) per second. smoothLevel: peak-hold + smoothing for raw audio.
        static Coeffs coeffs (float timeMs, double rate, bool smoothLevel)
        {
            const double t = timeMs * 0.001;
            Coeffs c;
            // Peak hold of 25 ms (half a period at 20 Hz) before release: a steady tone gives a flat level, so the gain never
            // ripples with the waveform. FFT bins need it too: phase interference makes a steady bin's magnitude flicker.
            c.peakHold = juce::jmax (smoothLevel ? 1 : 3, (int) std::ceil (0.025 * rate));
            c.pkRel = onePole (0.015, rate);
            if (smoothLevel) c.lvSm = onePole (0.0003, rate);
            else             c.floorDb = -115.0f;   // single FFT bins sit well below the broadband level
            c.slow = onePole (t, rate);
            c.hold = onePole (0.15 + 2.0 * t, rate);
            return c;
        }

        float pk = 0, lv = 0, slowDb = -120, holdDb = -120, mHold = 0;
        int holdLeft = 0;

        Masks run (float x, const Coeffs& c)
        {
            if (x >= pk)               { pk = x; holdLeft = c.peakHold; }
            else if (holdLeft > 0)     --holdLeft;
            else                       pk = x + c.pkRel * (pk - x);
            lv = pk + c.lvSm * (lv - pk);
            const float db = 20.0f * std::log10 (juce::jmax (lv, 1.0e-7f));
            slowDb = db + c.slow * (slowDb - db);
            holdDb = db > holdDb ? db : db + c.hold * (holdDb - db);
            const float gate = juce::jlimit (0.0f, 1.0f, (holdDb - c.floorDb) * 0.05f);   // ignore the noise floor
            const float m = gate * diffToMask (db - slowDb);
            mHold = m > mHold ? m : m + c.slow * (mHold - m);   // the transient region lasts about one Time
            return { m, gate * (1.0f - mHold) * diffToMask (holdDb - db) };
        }
    };
}
