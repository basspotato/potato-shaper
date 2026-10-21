#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

namespace ps
{
    namespace id
    {
        inline constexpr const char* mode      = "mode";       // Clean / Spectral
        inline constexpr const char* trans     = "trans";
        inline constexpr const char* sus       = "sus";
        inline constexpr const char* transSide = "transside";  // M/S mode: the Side channel's own Transient and Sustain
        inline constexpr const char* susSide   = "susside";
        inline constexpr const char* time      = "time";
        inline constexpr const char* warmth    = "warmth";
        inline constexpr const char* loCut     = "locut";
        inline constexpr const char* hiCut     = "hicut";
        inline constexpr const char* loQ       = "loq";
        inline constexpr const char* hiQ       = "hiq";
        inline constexpr const char* loSlope   = "loslope";
        inline constexpr const char* hiSlope   = "hislope";
        inline constexpr const char* lowF      = "lowf";
        inline constexpr const char* lowG      = "lowg";
        inline constexpr const char* lowQ      = "lowq";
        inline constexpr const char* midF      = "midf";
        inline constexpr const char* midG      = "midg";
        inline constexpr const char* midQ      = "midq";
        inline constexpr const char* highF     = "highf";
        inline constexpr const char* highG     = "highg";
        inline constexpr const char* highQ     = "highq";
        inline constexpr const char* lowType   = "lowtype";
        inline constexpr const char* midType   = "midtype";
        inline constexpr const char* highType  = "hightype";
        inline constexpr const char* focusProc = "focusproc";  // Focus band also limits where processing is applied
        inline constexpr const char* listen    = "listen";
        inline constexpr const char* msMode    = "msmode";     // L/R or M/S
        inline constexpr const char* link      = "link";
        inline constexpr const char* balance   = "balance";
        inline constexpr const char* extSc     = "extsc";
        inline constexpr const char* scGain    = "scgain";
        inline constexpr const char* makeup    = "makeup";
        inline constexpr const char* mix       = "mix";
        inline constexpr const char* clip      = "clip";
        inline constexpr const char* os        = "os";
        inline constexpr const char* window    = "window";     // Spectral only
        inline constexpr const char* sharp     = "sharp";      // Spectral only
        inline constexpr const char* bypass    = "bypass";
    }

    inline const juce::StringArray listenNames { "OUT", "TRANSIENT", "SUSTAIN", "DELTA", "SIDECHAIN" };
    inline const juce::StringArray bandTypeNames { "LO SHELF", "PEAK", "HI SHELF", "TILT" };
    inline const juce::StringArray hpNames     { "HP 6", "HP 12", "HP 24", "HP 48" };
    inline const juce::StringArray lpNames     { "LP 6", "LP 12", "LP 24", "LP 48" };
    inline const juce::StringArray modeNames   { "CLEAN", "SPECTRAL" };
    inline const juce::StringArray osNames     { "OFF", "2X", "4X" };
    inline const juce::StringArray windowNames { "SHORT", "MEDIUM", "LONG" };

    inline juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi, [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                         [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                         [] (float a, float b, float v) { return juce::jlimit (a, b, v); } };
    }

    inline juce::String fmtHz (float v)
    {
        return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz";
    }

    inline juce::String fmtDb (float v) { return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " dB"; }

    inline juce::String formatValue (const juce::String& pid, float v)
    {
        if (pid == id::trans || pid == id::sus || pid == id::transSide || pid == id::susSide || pid == id::makeup || pid == id::scGain
            || pid == id::lowG || pid == id::midG || pid == id::highG) return fmtDb (v);
        if (pid == id::time)   return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
        if (pid == id::warmth || pid == id::mix || pid == id::link) return juce::String (juce::roundToInt (v)) + " %";
        if (pid == id::balance)
        {
            const int a = juce::roundToInt (juce::jmin (100.0f, 100.0f - v)), b = juce::roundToInt (juce::jmin (100.0f, 100.0f + v));
            return juce::String (a) + " / " + juce::String (b);
        }
        if (pid == id::loCut || pid == id::hiCut || pid == id::lowF || pid == id::midF || pid == id::highF) return fmtHz (v);
        if (pid == id::sharp) return juce::String (v, 1);
        return juce::String (v, 2);
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout l;
        auto fl = [&] (const char* pid, const char* name, NormalisableRange<float> r, float def, const char* unit = "")
        {
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, r, def,
                AudioParameterFloatAttributes().withLabel (unit)
                    .withStringFromValueFunction ([p = String (pid)] (float v, int) { return formatValue (p, v); })));
        };
        auto choice = [&] (const char* pid, const char* name, const StringArray& opts, int def)
        {
            l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, opts, def));
        };
        auto toggle = [&] (const char* pid, const char* name, bool def)
        {
            l.add (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def));
        };
        auto qRange = [] { auto r = logRange (0.3f, 4.0f); return r; };

        choice (id::mode, "Mode", modeNames, 0);
        fl (id::trans,  "Transient", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB");
        fl (id::sus,    "Sustain",   { -24.0f, 24.0f, 0.01f }, 0.0f, "dB");
        fl (id::transSide, "Side Transient", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB");
        fl (id::susSide,   "Side Sustain",   { -24.0f, 24.0f, 0.01f }, 0.0f, "dB");
        fl (id::time,   "Time",      logRange (2.0f, 200.0f), 20.0f, "ms");
        fl (id::warmth, "Warmth",    { 0.0f, 100.0f, 0.1f }, 0.0f, "%");

        fl (id::loCut, "Lo Cut",    logRange (20.0f, 2000.0f), 20.0f, "Hz");
        fl (id::hiCut, "Hi Cut",    logRange (1000.0f, 20000.0f), 20000.0f, "Hz");
        fl (id::lowF,  "Low Freq",  logRange (30.0f, 500.0f), 90.0f, "Hz");
        fl (id::lowG,  "Low Gain",  { -18.0f, 18.0f, 0.01f }, 0.0f, "dB");
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id::lowQ, 1 }, "Low Width", qRange(), 0.71f));
        fl (id::midF,  "Mid Freq",  logRange (150.0f, 6000.0f), 800.0f, "Hz");
        fl (id::midG,  "Mid Gain",  { -18.0f, 18.0f, 0.01f }, 0.0f, "dB");
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id::midQ, 1 }, "Mid Width", qRange(), 0.71f));
        fl (id::highF, "High Freq", logRange (1500.0f, 18000.0f), 6000.0f, "Hz");
        fl (id::highG, "High Gain", { -18.0f, 18.0f, 0.01f }, 0.0f, "dB");
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id::highQ, 1 }, "High Width", qRange(), 0.71f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id::loQ, 1 }, "Lo Cut Q", qRange(), 0.71f));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id::hiQ, 1 }, "Hi Cut Q", qRange(), 0.71f));
        choice (id::loSlope,  "Lo Cut Slope", StringArray { "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct" }, slope24);
        choice (id::hiSlope,  "Hi Cut Slope", StringArray { "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct" }, slope24);
        choice (id::lowType,  "Low Type",  bandTypeNames, bandLoShelf);
        choice (id::midType,  "Mid Type",  bandTypeNames, bandPeak);
        choice (id::highType, "High Type", bandTypeNames, bandHiShelf);
        toggle (id::focusProc, "Focus Processing", true);

        choice (id::listen, "Listen", listenNames, 0);
        choice (id::msMode, "Stereo Mode", StringArray { "L/R", "M/S" }, 0);
        fl (id::link,    "Stereo Link", { 0.0f, 100.0f, 0.1f }, 100.0f, "%");
        fl (id::balance, "Balance",     { -100.0f, 100.0f, 0.1f }, 0.0f);
        toggle (id::extSc, "External Side-Chain", false);
        fl (id::scGain,  "Side-Chain Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB");
        fl (id::makeup,  "Makeup Gain", { -12.0f, 24.0f, 0.01f }, 0.0f, "dB");
        fl (id::mix,     "Mix",         { 0.0f, 100.0f, 0.1f }, 100.0f, "%");
        toggle (id::clip, "Safety Clip", true);
        choice (id::os, "Oversampling", osNames, 0);
        choice (id::window, "Window", windowNames, 1);
        fl (id::sharp,   "Sharpness",   { 0.0f, 10.0f, 0.01f }, 5.0f);
        toggle (id::bypass, "Bypass", false);
        return l;
    }
}
