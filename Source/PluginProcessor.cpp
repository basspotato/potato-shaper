#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int maxDelay = 1 << 15;   // covers the largest FFT (8192 at 192 kHz) plus 4x oversampling latency

    float peakOf (const float* x, int n)
    {
        float p = 0.0f;
        for (int i = 0; i < n; ++i) p = juce::jmax (p, std::abs (x[i]));
        return p;
    }

    void storeMax (std::atomic<float>& a, float v)
    {
        if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed);
    }
}

PotatoShaperProcessor::PotatoShaperProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PotatoShaper", ps::createLayout())
{
    auto r = [this] (const char* pid) { return apvts.getRawParameterValue (pid); };
    namespace id = ps::id;
    raw = { r (id::mode), r (id::trans), r (id::sus), r (id::time), r (id::warmth), r (id::loCut), r (id::hiCut), r (id::lowF),
            r (id::lowG), r (id::lowQ), r (id::midF), r (id::midG), r (id::midQ), r (id::highF), r (id::highG), r (id::highQ), r (id::lowType), r (id::midType), r (id::highType),
            r (id::focusProc), r (id::listen), r (id::msMode), r (id::link), r (id::balance), r (id::extSc), r (id::scGain),
            r (id::makeup), r (id::mix), r (id::clip), r (id::os), r (id::window), r (id::sharp), r (id::bypass) };
}

bool PotatoShaperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out || (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()))
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

ps::EngineParams PotatoShaperProcessor::readParams() const
{
    ps::EngineParams p;
    p.transDb = raw.trans->load();
    p.susDb   = raw.sus->load();
    p.timeMs  = raw.time->load();
    p.warmth  = raw.warmth->load() * 0.01f;
    p.focus   = { raw.loCut->load(), raw.hiCut->load(), raw.lowF->load(), raw.lowG->load(), raw.lowQ->load(),
                  raw.midF->load(), raw.midG->load(), raw.midQ->load(), raw.highF->load(), raw.highG->load(), raw.highQ->load(),
                  (int) raw.lowType->load(), (int) raw.midType->load(), (int) raw.highType->load() };
    p.focusProc = raw.focusProc->load() > 0.5f;
    p.link    = raw.link->load() * 0.01f;
    p.balance = raw.balance->load() * 0.01f;
    p.extSc   = raw.extSc->load() > 0.5f;
    p.scGain  = ps::dbToGain (raw.scGain->load());
    p.listen  = (int) raw.listen->load();
    p.window  = (int) raw.window->load();
    p.sharp   = raw.sharp->load();
    return p;
}

void PotatoShaperProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    maxChunk = juce::jmax (64, juce::jmin (samplesPerBlock, 2048));
    const int nCh = 2;

    clean.prepare (sampleRate);
    spectral.prepare (sampleRate);
    for (int i = 0; i < 2; ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>> (
            nCh, (size_t) (i + 1), juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversamplers[i]->initProcessing ((size_t) maxChunk);
    }

    dryBuf.setSize (nCh, maxChunk);
    scBuf.setSize (nCh, maxChunk);
    delayBuf.setSize (nCh, maxDelay);
    delayBuf.clear();
    delayPos = 0;
    mBuf.assign ((size_t) maxChunk, 0.0f);
    sBuf.assign ((size_t) maxChunk, 0.0f);

    makeupSm.reset (sampleRate, 0.03);
    mixSm.reset (sampleRate, 0.03);
    makeupSm.setCurrentAndTargetValue (ps::dbToGain (raw.makeup->load()));
    mixSm.setCurrentAndTargetValue (raw.mix->load() * 0.01f);
    for (int c = 0; c < 2; ++c) dcX[c] = dcY[c] = 0.0f;
    laneStep = juce::jmax (1, (int) (sampleRate * 0.005));
    laneCount = 0; laneAccM = laneAccS = 0.0f;
    lastMode = -1;
    latency = -1;
    updateLatency ((int) raw.mode->load(), (int) raw.window->load(), (int) raw.os->load());
}

void PotatoShaperProcessor::updateLatency (int mode, int window, int osIdx)
{
    engineLatency = mode == 1 ? ps::SpectralEngine::fftSizeFor (window, getSampleRate() > 0 ? getSampleRate() : sampleRateHz.load()) : 0;
    osLatency = osIdx > 0 && oversamplers[osIdx - 1] != nullptr ? (int) std::lround (oversamplers[osIdx - 1]->getLatencyInSamples()) : 0;
    const int total = engineLatency + osLatency;
    if (total != latency)
    {
        latency = total;
        setLatencySamples (total);
    }
}

void PotatoShaperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, true, 0);
    const int numCh = juce::jmin (2, main.getNumChannels(), getTotalNumOutputChannels());
    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, numSamples);
    if (numCh == 0 || numSamples == 0) return;

    const bool extSc = raw.extSc->load() > 0.5f;
    juce::AudioBuffer<float> sc;
    if (extSc && getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
        sc = getBusBuffer (buffer, true, 1);

    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const int n = juce::jmin (maxChunk, numSamples - start);
        float* io[2] = { main.getWritePointer (0, start), numCh > 1 ? main.getWritePointer (1, start) : nullptr };
        const float* scp[2] {};
        const float* const* scArg = nullptr;
        if (sc.getNumChannels() > 0)
        {
            scp[0] = sc.getReadPointer (0, start);
            scp[1] = sc.getReadPointer (juce::jmin (1, sc.getNumChannels() - 1), start);
            scArg = scp;
        }
        else if (extSc)
        {
            // External side-chain selected but the host gives no side-chain signal: detect nothing (silence)
            scBuf.clear();
            scp[0] = scBuf.getReadPointer (0); scp[1] = scBuf.getReadPointer (1);
            scArg = scp;
        }
        processChunk (io, scArg, numCh, n);
    }
}

void PotatoShaperProcessor::processChunk (float* const* io, const float* const* scIn, int numCh, int n)
{
    const int mode = (int) raw.mode->load();
    const int osIdx = juce::jlimit (0, 2, (int) raw.os->load());
    auto p = readParams();
    updateLatency (mode, p.window, osIdx);
    if (mode != lastMode)
    {
        clean.reset();
        spectral.reset();
        for (auto& o : oversamplers) if (o) o->reset();
        lastMode = mode;
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        storeMax (inPeak[ch], peakOf (io[ch], n));
        dryBuf.copyFrom (ch, 0, io[ch], n);
    }

    // Dry delay line (feeds latency-aligned dry for Mix, Delta and Bypass)
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = delayBuf.getWritePointer (ch);
        for (int i = 0; i < n; ++i) d[(delayPos + i) & (maxDelay - 1)] = io[ch][i];
    }
    auto delayed = [&] (int ch, int i, int lag) { return delayBuf.getSample (ch, (delayPos + i - lag) & (maxDelay - 1)); };

    // Mid/side encode (main and side-chain)
    const bool ms = numCh == 2 && raw.msMode->load() > 0.5f;
    const float* const* scUse = scIn;
    float* scw[2] = { scBuf.getWritePointer (0), scBuf.getWritePointer (1) };
    if (ms)
    {
        for (int i = 0; i < n; ++i)
        {
            const float l = io[0][i], r = io[1][i];
            io[0][i] = 0.5f * (l + r); io[1][i] = 0.5f * (l - r);
        }
        if (scIn != nullptr)
        {
            for (int i = 0; i < n; ++i)
            {
                const float l = scIn[0][i], r = scIn[1][i];
                scw[0][i] = 0.5f * (l + r); scw[1][i] = 0.5f * (l - r);
            }
            scUse = scw;
        }
    }

    float pkT = 0.0f, pkS = 0.0f;
    if (mode == 1) spectral.process (io, scUse, numCh, n, p, mBuf.data(), sBuf.data(), pkT, pkS);
    else           clean.process (io, scUse, numCh, n, p, mBuf.data(), sBuf.data(), pkT, pkS);

    if (ms)
        for (int i = 0; i < n; ++i)
        {
            const float m = io[0][i], s = io[1][i];
            io[0][i] = m + s; io[1][i] = m - s;
        }

    // Mix (against latency-aligned dry) and makeup; listen modes other than OUT pass the selected path unchanged
    const bool listenOut = p.listen == ps::listenOut || p.listen == ps::listenDelta;
    makeupSm.setTargetValue (ps::dbToGain (raw.makeup->load()));
    mixSm.setTargetValue (raw.mix->load() * 0.01f);
    for (int i = 0; i < n; ++i)
    {
        const float mk = makeupSm.getNextValue(), mx = mixSm.getNextValue();
        if (! listenOut) continue;
        for (int ch = 0; ch < numCh; ++ch)
            io[ch][i] = (delayed (ch, i, engineLatency) * (1.0f - mx) + io[ch][i] * mx) * mk;
    }

    // Warmth (saturation scaled by the transient mask) and safety clip, oversampled when selected
    const float w = listenOut ? p.warmth : 0.0f;
    const bool clip = raw.clip->load() > 0.5f;
    const float drive = 1.0f + 3.0f * w;
    auto shape = [&] (float x, float m)
    {
        if (w > 0.0f) x += m * w * (std::tanh (drive * x) / drive - x);
        return clip ? juce::jlimit (-1.0f, 1.0f, x) : x;
    };

    if (osIdx > 0)
    {
        auto& os = *oversamplers[osIdx - 1];
        juce::dsp::AudioBlock<float> block (io, (size_t) numCh, (size_t) n);
        auto up = os.processSamplesUp (block);
        const int f = 1 << osIdx;
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* x = up.getChannelPointer (ch);
            for (int i = 0; i < (int) up.getNumSamples(); ++i) x[i] = shape (x[i], mBuf[(size_t) (i / f)]);
        }
        os.processSamplesDown (block);
    }
    else if (w > 0.0f || clip)
    {
        for (int ch = 0; ch < numCh; ++ch)
            for (int i = 0; i < n; ++i) io[ch][i] = shape (io[ch][i], mBuf[(size_t) i]);
    }

    // Saturation can add a little DC: remove it (5 Hz high-pass) only when warmth is in use
    if (w > 0.0f)
    {
        const float a = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / getSampleRate());
        for (int ch = 0; ch < numCh; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float x = io[ch][i], y = x - dcX[ch] + a * dcY[ch];
                dcX[ch] = x; dcY[ch] = y; io[ch][i] = y;
            }
    }

    const bool bypass = raw.bypass->load() > 0.5f;
    bool didClip = false;
    for (int ch = 0; ch < numCh; ++ch)
        for (int i = 0; i < n; ++i)
        {
            float y = io[ch][i];
            if (bypass)                          y = delayed (ch, i, latency);
            else if (p.listen == ps::listenDelta) y = y - delayed (ch, i, latency);
            if (clip && ! bypass && std::abs (y) > 1.0f) { y = juce::jlimit (-1.0f, 1.0f, y); didClip = true; }
            io[ch][i] = y;
        }
    delayPos = (delayPos + n) & (maxDelay - 1);

    // Meters, scope and detector lane
    float rms = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
    {
        storeMax (outPeak[ch], peakOf (io[ch], n));
        for (int i = 0; i < n; ++i) rms += io[ch][i] * io[ch][i];
    }
    outRms.store (std::sqrt (rms / (float) (n * numCh)), std::memory_order_relaxed);
    if (numCh == 1) outPeak[1].store (outPeak[0].load());
    if (didClip) clipped = true;
    if (std::abs (pkT) > std::abs (transDb.load())) transDb = pkT;
    if (std::abs (pkS) > std::abs (susDb.load()))   susDb = pkS;

    int sp = scopePos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float v = numCh > 1 ? 0.5f * (io[0][i] + io[1][i]) : io[0][i];
        scope[(size_t) sp].store (v, std::memory_order_relaxed);
        sp = (sp + 1) & (scopeSize - 1);

        laneAccM = juce::jmax (laneAccM, mBuf[(size_t) i]);
        laneAccS = juce::jmax (laneAccS, sBuf[(size_t) i]);
        if (++laneCount >= laneStep)
        {
            const int lp = lanePos.load (std::memory_order_relaxed);
            laneM[(size_t) lp].store (laneAccM, std::memory_order_relaxed);
            laneS[(size_t) lp].store (laneAccS, std::memory_order_relaxed);
            lanePos.store ((lp + 1) % laneSize, std::memory_order_release);
            laneCount = 0; laneAccM = laneAccS = 0.0f;
        }
    }
    scopePos.store (sp, std::memory_order_release);
}

//==============================================================================
void PotatoShaperProcessor::selectAB (int slot)
{
    if (slot == abSlot) return;
    abStates[abSlot] = apvts.copyState();
    abSlot = slot;
    if (abStates[slot].isValid()) apvts.replaceState (abStates[slot].createCopy());
}

void PotatoShaperProcessor::copyABToOther()
{
    abStates[1 - abSlot] = apvts.copyState();
}

void PotatoShaperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void PotatoShaperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PotatoShaperProcessor::createEditor() { return new PotatoShaperEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PotatoShaperProcessor(); }
