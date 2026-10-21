#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Dsp.h"
#include <vector>
#include <memory>
#include <atomic>
#include <cstring>

namespace ps
{
    // Frequency-selective transient shaper. Each FFT bin runs the same Detector as the Clean engine, so only the frequencies that carry the transient (or the sustain) are boosted or cut.
    // Detection runs on the analytic signal (see Hilbert in Dsp.h) through a complex FFT.
    // STFT: periodic Hann analysis and synthesis windows, 75% overlap. Real, zero-phase bin gains (linear phase),
    // smoothed across frequency by Sharpness to keep time-domain aliasing low. Latency = FFT size.
    class SpectralEngine
    {
    public:
        static constexpr int maxCh = 2;
        static constexpr int displayPoints = 128;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            scale = sr > 96000.0 ? 4 : (sr > 48000.0 ? 2 : 1);
            maxN = 2048 * scale;
            ffts.clear();
            for (int o = 9; (1 << o) <= maxN; ++o) ffts.push_back (std::make_unique<juce::dsp::FFT> (o));

            const auto nb = (size_t) (maxN / 2 + 1);
            for (int ch = 0; ch < maxCh; ++ch)
            {
                inFifo[ch].assign ((size_t) maxN, 0.0f);
                scFifo[ch].assign ((size_t) maxN, {});
                outQ[ch].assign ((size_t) maxN, 0.0f);
                spec[ch].assign ((size_t) maxN * 2, 0.0f);
                scSpec[ch].assign ((size_t) maxN, {});
                mask[ch].assign (nb, 0.0f);
                smask[ch].assign (nb, 0.0f);
            }
            for (auto& e : env) e.assign (nb, {});
            mag.assign (nb, 0.0f);
            cIn.assign ((size_t) maxN, {});
            win.assign ((size_t) maxN, 0.0f);
            weight.assign (nb, 1.0f);
            band.assign (nb, 1.0f);
            n = 0;
            setWindow (1);
        }

        static int fftSizeFor (int window, double sampleRate)
        {
            const int sc = sampleRate > 96000.0 ? 4 : (sampleRate > 48000.0 ? 2 : 1);
            return (512 << juce::jlimit (0, 2, window)) * sc;
        }
        int getLatency() const { return n; }

        // Not real-time safe only in the sense of cost: it reallocates nothing, so it may run on the audio thread.
        void setWindow (int window)
        {
            const int newN = fftSizeFor (window, sr);
            if (newN == n) return;
            n = newN; hop = n / 4;
            order = 9 + window + (scale == 4 ? 2 : (scale == 2 ? 1 : 0));
            for (int i = 0; i < n; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) n);
            reset();
        }

        void reset()
        {
            for (int ch = 0; ch < maxCh; ++ch)
            {
                std::fill (inFifo[ch].begin(), inFifo[ch].end(), 0.0f);
                std::fill (scFifo[ch].begin(), scFifo[ch].end(), std::complex<float> {});
                hilbert[ch].reset();
                std::fill (outQ[ch].begin(), outQ[ch].end(), 0.0f);
                std::fill (mask[ch].begin(), mask[ch].end(), 0.0f);
                std::fill (smask[ch].begin(), smask[ch].end(), 0.0f);
            }
            for (auto& e : env) std::fill (e.begin(), e.end(), Detector {});
            pos = 0; hopCount = 0; frameM = frameS = 0.0f;
        }

        void process (float* const* io, const float* const* sc, int numCh, int numSamples, const EngineParams& p,
                      float* mOut, float* sOut, float& peakTransDb, float& peakSusDb)
        {
            numCh = juce::jmin (numCh, maxCh);
            setWindow (p.window);
            params = &p;

            for (int i = 0; i < numSamples; ++i)
            {
                for (int ch = 0; ch < numCh; ++ch)
                {
                    inFifo[ch][(size_t) pos] = io[ch][i];
                    scFifo[ch][(size_t) pos] = hilbert[ch].process (sc != nullptr ? sc[ch][i] * p.scGain : io[ch][i]);
                    io[ch][i] = outQ[ch][(size_t) pos];
                    outQ[ch][(size_t) pos] = 0.0f;
                }
                mOut[i] = frameM; sOut[i] = frameS;
                pos = (pos + 1) % n;

                if (++hopCount == hop)
                {
                    hopCount = 0;
                    processFrame (numCh, peakTransDb, peakSusDb);
                }
            }
        }

        // Average bin gain in dB at log-spaced display frequencies (20 Hz .. 20 kHz), written by the audio thread
        std::array<std::atomic<float>, displayPoints> display {};

    private:
        void processFrame (int numCh, float& peakTransDb, float& peakSusDb)
        {
            const auto& p = *params;
            const int nb = n / 2 + 1;
            const double frameRate = sr / hop;
            auto* fft = ffts[(size_t) (order - 9)].get();

            // Focus weighting per bin: the curve scales how strongly each bin is processed (a side-chain EQ, not an EQ)
            // (recomputed only when the curve changes: evaluating it for every bin is not free)
            if (std::memcmp (&p.focus, &lastFocus, sizeof (FocusSettings)) != 0 || p.focusProc != lastFocusProc || weightN != n)
            {
                lastFocus = p.focus; lastFocusProc = p.focusProc; weightN = n;
                curve.design (p.focus, sr);
                const bool useBand = p.focusProc && curve.bandActive();
                for (int k = 0; k < nb; ++k)
                {
                    const double f = juce::jmax (1.0, (double) k * sr / n);
                    weight[(size_t) k] = (float) juce::jmin (2.0, curve.magnitude (f, sr));
                    band[(size_t) k] = useBand ? (float) curve.bandMagnitude (f, sr) : 1.0f;
                }
            }

            const auto dc = Detector::coeffs (p.timeMs, frameRate, false);
            const float norm = 2.0f / (float) n;   // analytic bin magnitude -> sine amplitude for a Hann window

            // Analysis (main and detector source)
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* s = spec[ch].data();
                for (int i = 0; i < n; ++i)
                {
                    const int idx = (pos + i) % n;   // oldest sample first
                    s[i] = inFifo[ch][(size_t) idx] * win[(size_t) i];
                    cIn[(size_t) i] = scFifo[ch][(size_t) idx] * win[(size_t) i];
                }
                std::fill (s + n, s + 2 * n, 0.0f);
                fft->performRealOnlyForwardTransform (s, true);
                fft->perform (cIn.data(), scSpec[ch].data(), false);
            }

            // Detection per bin: per channel, and on the linked (power-averaged) magnitude
            float sumM = 0, sumS = 0, wSum = 1.0e-9f;
            for (int k = 0; k < nb; ++k)
            {
                float pw = 0.0f, own[maxCh][2] {};
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float m2 = std::norm (scSpec[ch][(size_t) k]);
                    pw += m2;
                    { const auto r = env[ch][(size_t) k].run (norm * std::sqrt (m2), dc); own[ch][0] = r.m; own[ch][1] = r.s; }
                }
                float lk[2] { own[0][0], own[0][1] };
                if (numCh > 1) { const auto r = env[maxCh][(size_t) k].run (norm * std::sqrt (pw / (float) numCh), dc); lk[0] = r.m; lk[1] = r.s; }

                const float w = weight[(size_t) k];
                for (int ch = 0; ch < numCh; ++ch)
                {
                    mask[ch][(size_t) k]  = w * (own[ch][0] + (lk[0] - own[ch][0]) * p.link);
                    smask[ch][(size_t) k] = w * (own[ch][1] + (lk[1] - own[ch][1]) * p.link);
                }
                mag[(size_t) k] = std::sqrt (pw);
            }

            // Sharpness: smooth the masks across frequency (forward-backward one-pole). Low sharpness = wide, smooth bands.
            const float c = juce::jlimit (0.0f, 0.97f, 0.92f - 0.09f * p.sharp);
            for (int ch = 0; ch < numCh; ++ch)
                for (auto* v : { mask[ch].data(), smask[ch].data() })
                {
                    for (int k = 1; k < nb; ++k)      v[k] += c * (v[k - 1] - v[k]);
                    for (int k = nb - 2; k >= 0; --k) v[k] += c * (v[k + 1] - v[k]);
                }

            // Gains and synthesis
            float dispAcc[displayPoints] {}; int dispCnt[displayPoints] {};
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float amt = numCh > 1 ? balanceAmount (p.balance, ch) : 1.0f;
                auto* s = spec[ch].data();
                for (int k = 0; k < nb; ++k)
                {
                    const float m = mask[ch][(size_t) k] * band[(size_t) k], sm = smask[ch][(size_t) k] * band[(size_t) k];
                    float gdb = juce::jlimit (-36.0f, 30.0f, amt * (m * p.transDb + sm * p.susDb));
                    float g;
                    switch (p.listen)
                    {
                        case listenTrans: g = juce::jmin (1.0f, m); break;
                        case listenSus:   g = juce::jmin (1.0f, sm); break;
                        case listenSc:
                            g = weight[(size_t) k];
                            if (p.extSc) { s[2 * k] = 0.5f * scSpec[ch][(size_t) k].real(); s[2 * k + 1] = 0.5f * scSpec[ch][(size_t) k].imag(); }
                            break;
                        default:          g = dbToGain (gdb);
                    }
                    s[2 * k] *= g; s[2 * k + 1] *= g;

                    const float wk = mag[(size_t) k];
                    sumM += m * wk; sumS += sm * wk; wSum += wk;
                    peakTransDb = std::abs (amt * m * p.transDb) > std::abs (peakTransDb) ? amt * m * p.transDb : peakTransDb;
                    peakSusDb   = std::abs (amt * sm * p.susDb)  > std::abs (peakSusDb)   ? amt * sm * p.susDb   : peakSusDb;

                    const double f = (double) k * sr / n;
                    if (f >= 20.0 && f <= 20000.0)
                    {
                        const int di = juce::jlimit (0, displayPoints - 1, (int) (displayPoints * std::log (f / 20.0) / std::log (1000.0)));
                        dispAcc[di] += gdb; ++dispCnt[di];
                    }
                }
                fft->performRealOnlyInverseTransform (s);

                // Overlap-add starting at the next sample to be read; Hann^2 at 75% overlap sums to 1.5
                for (int i = 0; i < n; ++i)
                    outQ[ch][(size_t) ((pos + i) % n)] += s[i] * win[(size_t) i] * (1.0f / 1.5f);
            }

            // Fill display bins that had no FFT bin (low frequencies) from their neighbours
            float last = 0.0f;
            for (int d = 0; d < displayPoints; ++d)
            {
                if (dispCnt[d] > 0) last = dispAcc[d] / (float) dispCnt[d];
                display[(size_t) d].store (last, std::memory_order_relaxed);
            }

            frameM = juce::jlimit (0.0f, 1.0f, sumM / wSum);
            frameS = juce::jlimit (0.0f, 1.0f, sumS / wSum);
        }

        double sr = 44100.0;
        int scale = 1, maxN = 2048, n = 0, hop = 0, order = 10, pos = 0, hopCount = 0;
        std::vector<std::unique_ptr<juce::dsp::FFT>> ffts;
        std::vector<float> inFifo[maxCh], outQ[maxCh], spec[maxCh], mask[maxCh], smask[maxCh];
        std::vector<std::complex<float>> scFifo[maxCh], scSpec[maxCh], cIn;
        Hilbert hilbert[maxCh];
        std::vector<Detector> env[maxCh + 1];
        std::vector<float> mag, win, weight, band;
        FocusCurve curve;
        FocusSettings lastFocus { -1.0f };
        bool lastFocusProc = false;
        int weightN = 0;
        const EngineParams* params = nullptr;
        float frameM = 0, frameS = 0;
    };
}
