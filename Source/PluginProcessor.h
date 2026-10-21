#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "CleanEngine.h"
#include "SpectralEngine.h"
#include "Presets.h"

class PotatoShaperProcessor : public juce::AudioProcessor
{
public:
    PotatoShaperProcessor();
    ~PotatoShaperProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Potato-Shaper"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Potato-Shaper"; }
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (ps::id::bypass); }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // A/B comparison (message thread only)
    void selectAB (int slot);
    void copyABToOther();
    int getABSlot() const { return abSlot; }

    // Factory presets (message thread). -1 = INIT, no preset loaded
    void loadPreset (int index, bool asUserGesture = true);
    bool presetIsModified() const;
    std::atomic<int> currentPreset { -1 };

    juce::AudioProcessorValueTreeState apvts;

    // Metering and display data: written by the audio thread, read by the editor
    std::atomic<float> inPeak[2] {}, outPeak[2] {}, outRms { 0.0f };
    std::atomic<float> transDb { 0.0f }, susDb { 0.0f };
    std::atomic<bool>  clipped { false };

    static constexpr int scopeSize = 1 << 14;
    int getScopeWritePos() const noexcept { return scopePos.load (std::memory_order_acquire); }
    void copyScope (float* dest, int num, int endPos) const noexcept
    {
        const int start = (endPos - num) & (scopeSize - 1);
        for (int i = 0; i < num; ++i)
            dest[i] = scope[(size_t) ((start + i) & (scopeSize - 1))].load (std::memory_order_relaxed);
    }

    static constexpr int laneSize = 512;   // detector history, one entry per 5 ms
    std::array<std::atomic<float>, laneSize> laneM {}, laneS {};
    std::atomic<int> lanePos { 0 };

    const std::array<std::atomic<float>, ps::SpectralEngine::displayPoints>& spectralDisplay() const { return spectral.display; }
    double getRate() const { return sampleRateHz.load(); }

private:
    ps::EngineParams readParams() const;
    void processChunk (float* const* io, const float* const* sc, int numCh, int n);
    void updateLatency (int mode, int window, int osIdx);

    ps::CleanEngine clean;
    ps::SpectralEngine spectral;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[2];
    int lastMode = -1, latency = 0, engineLatency = 0, osLatency = 0;

    juce::AudioBuffer<float> dryBuf, scBuf, delayBuf;
    std::vector<float> mBuf, sBuf;
    int maxChunk = 512, delayPos = 0;
    juce::SmoothedValue<float> makeupSm, mixSm;
    float dcX[2] {}, dcY[2] {};
    int laneCount = 0, laneStep = 240;
    float laneAccM = 0, laneAccS = 0;
    std::atomic<double> sampleRateHz { 44100.0 };

    std::array<std::atomic<float>, scopeSize> scope {};
    std::atomic<int> scopePos { 0 };

    juce::RangedAudioParameter* presetParams[ps::numPresetParams] {};

    juce::ValueTree abStates[2];
    int abSlot = 0;

    struct Raw
    {
        std::atomic<float> *mode, *trans, *sus, *time, *warmth, *loCut, *hiCut, *lowF, *lowG, *lowQ, *midF, *midG, *midQ,
                           *highF, *highG, *highQ, *transSide, *susSide, *lowType, *midType, *highType, *loQ, *hiQ, *loSlope, *hiSlope, *focusProc, *listen, *msMode, *link, *balance, *extSc, *scGain,
                           *makeup, *mix, *clip, *os, *window, *sharp, *bypass;
    } raw {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PotatoShaperProcessor)
};
