#pragma once
#include "Dsp.h"

namespace ps
{
    // Zero-latency broadband transient shaper. The detector (see Detector in Dsp.h) listens to the input, or the external
    // side-chain, through the Focus curve; its transient and sustain masks set one gain per channel.
    class CleanEngine
    {
    public:
        static constexpr int maxCh = 2;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            reset();
        }

        void reset()
        {
            for (auto& ch : det) for (auto& b : ch) b.reset();
            for (auto& ch : bandF) for (auto& b : ch) b.reset();
            for (auto& e : env) e = {};
            for (auto& g : gainDb) g = 0.0f;
        }

        // io: main channels in L/R or M/S, processed in place. sc: detector source channels when external side-chain is on.
        // mOut/sOut receive the per-sample transient and sustain masks (averaged over channels).
        void process (float* const* io, const float* const* sc, int numCh, int n, const EngineParams& p,
                      float* mOut, float* sOut, float& peakTransDb, float& peakSusDb)
        {
            numCh = juce::jmin (numCh, maxCh);
            curve.design (p.focus, sr);
            for (int s = 0; s < FocusCurve::numStages; ++s)   // a stage that was off starts from silence when it comes back
                if (! curve.active[(size_t) s]) for (auto& ch : det) ch[s].reset();
            for (int s = 0; s < FocusCurve::cutStages; ++s)
                for (auto& ch : bandF)
                {
                    if (! curve.active[(size_t) (FocusCurve::loStage + s)]) ch[s].reset();
                    if (! curve.active[(size_t) (FocusCurve::hiStage + s)]) ch[FocusCurve::cutStages + s].reset();
                }
            const auto dc = Detector::coeffs (p.timeMs, sr, true);
            const float aGain = onePole (0.0002, sr);
            const bool useBand = p.focusProc && curve.bandActive();

            for (int i = 0; i < n; ++i)
            {
                float d[maxCh] = {};
                float pw = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    float v = (sc != nullptr ? sc[ch][i] * p.scGain : io[ch][i]);
                    for (int s = 0; s < FocusCurve::numStages; ++s)
                        if (curve.active[(size_t) s]) v = det[ch][s].process (curve.c[(size_t) s], v);
                    d[ch] = v;
                    pw += v * v;
                }

                Detector::Masks own[maxCh];
                for (int ch = 0; ch < numCh; ++ch)
                    own[ch] = env[ch].run (std::abs (d[ch]), dc);
                const auto linked = numCh > 1 ? env[maxCh].run (std::sqrt (pw / (float) numCh), dc) : own[0];

                float mSum = 0.0f, sSum = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float m = own[ch].m + (linked.m - own[ch].m) * p.link;
                    const float s = own[ch].s + (linked.s - own[ch].s) * p.link;
                    mSum += m; sSum += s;

                    const float amt = numCh > 1 ? balanceAmount (p.balance, ch) : 1.0f;
                    const float target = amt * (m * p.transDb[ch] + s * p.susDb[ch]);
                    gainDb[ch] = target + aGain * (gainDb[ch] - target);
                    peakTransDb = std::abs (amt * m * p.transDb[ch]) > std::abs (peakTransDb) ? amt * m * p.transDb[ch] : peakTransDb;
                    peakSusDb   = std::abs (amt * s * p.susDb[ch])   > std::abs (peakSusDb)   ? amt * s * p.susDb[ch]   : peakSusDb;

                    const float x = io[ch][i];
                    float b = x;
                    if (useBand)
                        for (int k = 0; k < FocusCurve::cutStages; ++k)
                        {
                            if (curve.active[(size_t) (FocusCurve::loStage + k)]) b = bandF[ch][k].process (curve.c[(size_t) (FocusCurve::loStage + k)], b);
                            if (curve.active[(size_t) (FocusCurve::hiStage + k)]) b = bandF[ch][FocusCurve::cutStages + k].process (curve.c[(size_t) (FocusCurve::hiStage + k)], b);
                        }

                    float y;
                    switch (p.listen)
                    {
                        case listenTrans: y = b * m; break;
                        case listenSus:   y = b * s; break;
                        case listenSc:    y = d[ch]; break;
                        default:
                        {
                            const float g = dbToGain (gainDb[ch]);
                            y = useBand ? x + (g - 1.0f) * b : x * g;
                        }
                    }
                    io[ch][i] = y;
                }
                mOut[i] = mSum / (float) numCh;
                sOut[i] = sSum / (float) numCh;
            }
        }

        const FocusCurve& getCurve() const { return curve; }

    private:
        double sr = 44100.0;
        FocusCurve curve;
        Biquad det[maxCh][FocusCurve::numStages];
        Biquad bandF[maxCh][2 * FocusCurve::cutStages];   // the cuts again, on the audio, for Focus PROC
        Detector env[maxCh + 1];   // per channel plus the linked (mono) detector
        float gainDb[maxCh] {};
    };
}
