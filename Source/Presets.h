#pragma once
#include "Params.h"

// 90 factory presets for EDM: 9 groups of 10. Side-chain source, side-chain gain and bypass are left as they are; Listen goes back to OUT.
// Starting points (from common EDM practice): drums get +3..+8 dB attack with sustain cut, sources keep body in their own band
// via the Focus curve, mix and master work stay at +1..+3 dB in the Spectral engine, imaging uses M/S with Balance to pick the
// channel (-100 = mid only, +100 = side only) or, better, set the Mid and Side separately. Lo / hi cut slopes: steep (48) to keep bleed out of drums and subs,
// gentle (6 / 12) where the band edge should stay soft.
namespace ps
{
    struct Preset
    {
        const char* name;
        int mode = 0;
        float trans = 0.0f, sus = 0.0f, time = 20.0f, warmth = 0.0f, loCut = 20.0f, hiCut = 20000.0f;
        float loQ = 0.71f, hiQ = 0.71f; int loSlope = slope24, hiSlope = slope24;
        float lowF = 90.0f, lowG = 0.0f, lowQ = 0.71f;   int lowType = bandLoShelf;
        float midF = 800.0f, midG = 0.0f, midQ = 0.71f;  int midType = bandPeak;
        float highF = 6000.0f, highG = 0.0f, highQ = 0.71f; int highType = bandHiShelf;
        bool focusProc = true;
        int msMode = 0;
        float link = 100.0f, balance = 0.0f, makeup = 0.0f, mix = 100.0f;
        bool clip = true;
        int os = 0, window = 1;
        float sharp = 5.0f;
        float transSide = 0.0f, susSide = 0.0f;   // M/S only: the Side channel's own settings (trans / sus are the Mid's)

        constexpr explicit Preset (const char* n) : name (n) {}

        // Builder steps, so each preset line reads like its settings
        constexpr Preset ts (float t, float s, float ms) const { auto p = *this; p.trans = t; p.sus = s; p.time = ms; return p; }
        constexpr Preset warm (float w) const                  { auto p = *this; p.warmth = w; return p; }
        constexpr Preset spec (int win = 1, float sh = 5.0f) const { auto p = *this; p.mode = 1; p.window = win; p.sharp = sh; return p; }
        constexpr Preset cuts (float lo, float hi) const       { auto p = *this; p.loCut = lo; p.hiCut = hi; return p; }
        constexpr Preset slopes (int lo, int hi) const          { auto p = *this; p.loSlope = lo; p.hiSlope = hi; return p; }
        constexpr Preset low (float f, float g, int type = bandLoShelf, float q = 0.71f) const { auto p = *this; p.lowF = f; p.lowG = g; p.lowType = type; p.lowQ = q; return p; }
        constexpr Preset mid (float f, float g, float q = 0.71f, int type = bandPeak) const   { auto p = *this; p.midF = f; p.midG = g; p.midQ = q; p.midType = type; return p; }
        constexpr Preset high (float f, float g, int type = bandHiShelf, float q = 0.71f) const { auto p = *this; p.highF = f; p.highG = g; p.highType = type; p.highQ = q; return p; }
        constexpr Preset ms (float bal, float lnk = 0.0f) const { auto p = *this; p.msMode = 1; p.balance = bal; p.link = lnk; return p; }
        constexpr Preset side (float t, float s) const          { auto p = *this; p.transSide = t; p.susSide = s; return p; }
        constexpr Preset lr (float lnk) const                   { auto p = *this; p.link = lnk; return p; }
        constexpr Preset gain (float db) const                  { auto p = *this; p.makeup = db; return p; }
        constexpr Preset wet (float m) const                    { auto p = *this; p.mix = m; return p; }
        constexpr Preset ovs (int o) const                      { auto p = *this; p.os = o; return p; }
    };

    inline constexpr int numGroups = 9;
    inline constexpr int presetsPerGroup = 10;
    inline constexpr int numPresets = numGroups * presetsPerGroup;
    inline const char* const groupNames[numGroups] = { "EDM", "Bass", "Synth", "Kick", "Hat", "Clap", "Vocals", "Imaging", "Master" };

    inline const Preset presets[numPresets] =
    {
        // EDM: drum buses, loops and groups
        Preset ("Festival Drum Bus").ts (5.0f, -3.0f, 12.0f).warm (20).low (80, 3).high (6000, 2).ovs (1),
        Preset ("Big Room Slam").ts (8.0f, -4.0f, 10.0f).warm (35).low (70, 4).high (5000, 3).gain (-1.0f).ovs (2),
        Preset ("Tech House Groove").ts (3.0f, -2.0f, 18.0f).warm (10).mid (1200, 3, 0.8f),
        Preset ("Progressive Glue").ts (2.0f, 1.5f, 30.0f).warm (10).spec (2, 4.0f).wet (70),
        Preset ("Future Bounce").ts (6.0f, -5.0f, 8.0f).warm (25).low (90, 2).high (7000, 3).gain (1.0f),
        Preset ("Dubstep Smack").ts (7.0f, -2.0f, 10.0f).warm (40).mid (1800, 4, 1.0f).ovs (2),
        Preset ("DnB Break Snap").ts (6.0f, -4.0f, 5.0f).warm (15).mid (2500, 4).high (8000, 3),
        Preset ("Melodic Air").ts (2.0f, 3.0f, 40.0f).high (8000, 4).spec (2, 4.0f).wet (80),
        Preset ("Trance Drive").ts (4.0f, -2.0f, 15.0f).warm (20).low (100, 2),
        Preset ("Parallel Punch").ts (10.0f, -6.0f, 10.0f).warm (30).wet (40).ovs (1),

        // Bass
        Preset ("Sub Tighten").ts (2.0f, -5.0f, 25.0f).low (80, 4).cuts (20, 1000).slopes (slope24, slope48).gain (1.0f),
        Preset ("Pluck Bass Snap").ts (6.0f, -3.0f, 10.0f).warm (15).mid (700, 4),
        Preset ("Reese Bite").ts (4.0f, -1.0f, 15.0f).warm (30).mid (1500, 4).spec (1, 6.0f),
        Preset ("Growl Accents").ts (6.0f, -2.0f, 8.0f).warm (35).mid (1200, 5, 0.8f).spec (1, 7.0f).ovs (1),
        Preset ("808 Knock").ts (5.0f, 2.0f, 15.0f).warm (25).low (60, 3).cuts (20, 3000).slopes (slope24, slope12),
        Preset ("Rolling Bass").ts (3.0f, -4.0f, 20.0f).warm (10).cuts (20, 4000).gain (1.0f),
        Preset ("Wobble Clarity").ts (3.0f, -2.0f, 12.0f).warm (20).mid (900, 3).spec(),
        Preset ("Bass Tail Trim").ts (0.0f, -8.0f, 30.0f).gain (1.5f),
        Preset ("Neuro Edge").ts (7.0f, -3.0f, 6.0f).warm (40).high (3000, 4).spec (1, 8.0f).ovs (2),
        Preset ("Deep House Round").ts (-2.0f, 3.0f, 30.0f).warm (20).cuts (20, 6000).slopes (slope24, slope6),

        // Synth
        Preset ("Pluck Pop").ts (5.0f, -4.0f, 10.0f).warm (15).mid (1500, 3),
        Preset ("Supersaw Focus").ts (3.0f, -1.0f, 15.0f).warm (10).mid (2000, 3).spec(),
        Preset ("Stab Hit").ts (6.0f, -3.0f, 8.0f).warm (25).mid (1000, 4, 0.9f),
        Preset ("Lead Cut").ts (3.0f, -2.0f, 12.0f).warm (20).mid (1500, 4),
        Preset ("Pad Bloom").ts (-3.0f, 5.0f, 40.0f).spec (2, 3.0f),
        Preset ("Arp Sparkle").ts (4.0f, -3.0f, 6.0f).warm (10).high (5000, 4),
        Preset ("Chord Stab Tight").ts (4.0f, -5.0f, 10.0f).warm (15).gain (1.0f),
        Preset ("Hoover Thrust").ts (5.0f, 0.0f, 15.0f).warm (35).mid (800, 3).spec().ovs (1),
        Preset ("Keys Natural").ts (2.0f, -1.0f, 20.0f).warm (5).wet (80),
        Preset ("Gated Chop").ts (3.0f, -10.0f, 6.0f).gain (2.0f),

        // Kick
        Preset ("Big Room Click").ts (7.0f, -2.0f, 6.0f).warm (20).high (4000, 4).ovs (1),
        Preset ("Tech Thump").ts (4.0f, -3.0f, 12.0f).warm (15).low (70, 3),
        Preset ("Kick Tail Trim").ts (0.0f, -8.0f, 15.0f).gain (1.0f),
        Preset ("Hardstyle Punch").ts (8.0f, 2.0f, 8.0f).warm (50).mid (1500, 3).gain (-1.0f).ovs (2),
        Preset ("Sub Boom").ts (-1.0f, 4.0f, 20.0f).warm (10).low (55, 4).cuts (20, 2000).slopes (slope24, slope48),
        Preset ("Techno Rumble").ts (3.0f, 3.0f, 15.0f).warm (25).low (60, 3),
        Preset ("Trap 808 Hit").ts (5.0f, 2.0f, 10.0f).warm (20).low (60, 2),
        Preset ("House Knock").ts (5.0f, -4.0f, 10.0f).warm (15).mid (250, 3),
        Preset ("Soft Kick Tame").ts (-4.0f, 0.0f, 10.0f),
        Preset ("Layered Glue").ts (3.0f, -2.0f, 15.0f).warm (30).wet (60),

        // Hat
        Preset ("Hat Crisp").ts (4.0f, -3.0f, 3.0f).cuts (2000, 20000).slopes (slope48, slope24).high (8000, 3),
        Preset ("Open Hat Tighten").ts (2.0f, -8.0f, 6.0f).cuts (2000, 20000).gain (1.0f),
        Preset ("Shaker Groove").ts (3.0f, -4.0f, 4.0f).cuts (1500, 20000).slopes (slope12, slope24),
        Preset ("Soft Tops").ts (-4.0f, 1.0f, 3.0f),
        Preset ("Ride Shimmer").ts (2.0f, 4.0f, 10.0f).cuts (1000, 20000).slopes (slope12, slope24),
        Preset ("Trap Roll Snap").ts (6.0f, -4.0f, 2.0f).ovs (1),
        Preset ("Hat Sizzle Down").ts (-3.0f, -2.0f, 4.0f).high (10000, 4).spec (0, 7.0f),
        Preset ("Top Loop Punch").ts (5.0f, -3.0f, 5.0f).warm (10).cuts (800, 20000).slopes (slope6, slope24),
        Preset ("Side Hat Tame").ts (-2.0f, -4.0f, 4.0f).ms (100.0f),
        Preset ("Lo-Fi Tops").ts (3.0f, -2.0f, 5.0f).warm (60),

        // Clap
        Preset ("Clap Snap").ts (5.0f, -3.0f, 8.0f).warm (15).mid (1500, 3).cuts (200, 20000),
        Preset ("Dry Clap").ts (2.0f, -9.0f, 10.0f).gain (1.0f),
        Preset ("Big Room Clap").ts (6.0f, 2.0f, 10.0f).warm (30).ovs (1),
        Preset ("Snare Crack").ts (6.0f, -3.0f, 6.0f).warm (20).mid (2500, 4).cuts (150, 20000).slopes (slope12, slope24),
        Preset ("Room Clap").ts (-1.0f, 6.0f, 15.0f),
        Preset ("Trap Clap Tail").ts (3.0f, -5.0f, 10.0f).warm (10),
        Preset ("Rim Click").ts (7.0f, -6.0f, 3.0f).warm (10).high (4000, 3),
        Preset ("Clap Layer Glue").ts (3.0f, -1.0f, 12.0f).warm (25).wet (70),
        Preset ("Snare Body").ts (2.0f, 3.0f, 20.0f).warm (20).mid (250, 3),
        Preset ("Soft Clap").ts (-4.0f, 0.0f, 10.0f),

        // Vocals
        Preset ("Vocal Presence").ts (3.0f, -1.0f, 12.0f).mid (3000, 3).cuts (150, 20000).slopes (slope12, slope24).spec().wet (60),
        Preset ("Room Remover").ts (1.0f, -6.0f, 15.0f).cuts (150, 20000).slopes (slope48, slope24).spec().gain (1.0f),
        Preset ("Rap Punch").ts (4.0f, -2.0f, 10.0f).warm (10).mid (2500, 3),
        Preset ("Vocal Chop Snap").ts (6.0f, -5.0f, 6.0f).warm (15).gain (1.0f),
        Preset ("Breathy Soften").ts (-3.0f, 2.0f, 10.0f).high (6000, 3).spec(),
        Preset ("Pop Vocal Polish").ts (2.0f, 1.0f, 20.0f).warm (15).spec().wet (70),
        Preset ("Ad-Lib Bite").ts (5.0f, -2.0f, 8.0f).warm (30),
        Preset ("Choir Bloom").ts (-2.0f, 4.0f, 40.0f).spec (2, 4.0f),
        Preset ("Consonant Lift").ts (4.0f, 0.0f, 5.0f).high (5000, 4).spec (0, 6.0f).wet (50),
        Preset ("Vocal Bus Tight").ts (2.0f, -3.0f, 15.0f).warm (10).spec().wet (80),

        // Imaging: M/S with unlinked detection. The first T / S pair is the Mid, .side() sets the Side
        Preset ("Punch & Space").ts (4.0f, -2.0f, 12.0f).side (-3.0f, 2.0f).ms (0.0f),
        Preset ("Wide Tails").ts (0.0f, 0.0f, 25.0f).side (0.0f, 4.0f).ms (0.0f),
        Preset ("Wide Attack").ts (0.0f, 0.0f, 8.0f).side (4.0f, 0.0f).ms (0.0f),
        Preset ("Side Tame").ts (0.0f, 0.0f, 10.0f).side (-3.0f, -3.0f).ms (0.0f),
        Preset ("Center Focus").ts (3.0f, -3.0f, 15.0f).side (-2.0f, 0.0f).mid (1000, 3).ms (0.0f),
        Preset ("Ambience Push").ts (0.0f, -2.0f, 40.0f).side (-2.0f, 6.0f).spec (2, 4.0f).ms (0.0f),
        Preset ("Dry Sides").ts (2.0f, 0.0f, 20.0f).side (0.0f, -6.0f).ms (0.0f),
        Preset ("Free Stereo Hits").ts (4.0f, -2.0f, 10.0f).lr (0.0f),
        Preset ("Mono Low Tight").ts (2.0f, -4.0f, 20.0f).side (-3.0f, -6.0f).low (80, 3).ms (0.0f),
        Preset ("Side Air Sparkle").ts (1.0f, 0.0f, 6.0f).side (4.0f, 2.0f).cuts (2000, 20000).high (8000, 3).slopes (slope12, slope24).ms (0.0f),

        // Master: Spectral engine, gentle amounts
        Preset ("Gentle Punch").ts (1.5f, 0.0f, 25.0f).spec().ovs (1),
        Preset ("Club Master").ts (2.5f, -1.0f, 20.0f).warm (10).low (80, 2).spec().ovs (1),
        Preset ("Loud & Open").ts (2.0f, 1.0f, 30.0f).warm (15).spec().ovs (2),
        Preset ("Transient Restore").ts (3.0f, 0.0f, 15.0f).spec (0, 6.0f).ovs (1),
        Preset ("Low End Control").ts (1.0f, -2.0f, 30.0f).low (80, 3).cuts (20, 1000).slopes (slope24, slope12).spec(),
        Preset ("Smooth Out").ts (-2.0f, 1.0f, 20.0f).spec(),
        Preset ("Festival Master").ts (3.0f, -1.0f, 15.0f).warm (20).spec().ovs (2),
        Preset ("Air & Snap").ts (1.5f, 0.0f, 10.0f).high (8000, 3).spec(),
        Preset ("Mid Punch Master").ts (2.0f, 0.0f, 20.0f).spec().ms (-100.0f),
        Preset ("Stem Glue").ts (1.0f, 1.0f, 40.0f).warm (10).wet (50),
    };

    // Parameters a preset sets, in the order presetValues() fills them
    inline const char* const presetParamIds[] = { id::mode, id::trans, id::sus, id::transSide, id::susSide, id::time, id::warmth, id::loCut, id::hiCut,
        id::loQ, id::hiQ, id::loSlope, id::hiSlope, id::lowF, id::lowG, id::lowQ, id::lowType, id::midF, id::midG, id::midQ, id::midType, id::highF, id::highG, id::highQ, id::highType,
        id::focusProc, id::listen, id::msMode, id::link, id::balance, id::makeup, id::mix, id::clip, id::os, id::window, id::sharp };
    inline constexpr int numPresetParams = (int) (sizeof (presetParamIds) / sizeof (presetParamIds[0]));

    inline void presetValues (const Preset& p, float* v)
    {
        const float vals[numPresetParams] = { (float) p.mode, p.trans, p.sus, p.transSide, p.susSide, p.time, p.warmth, p.loCut, p.hiCut,
            p.loQ, p.hiQ, (float) p.loSlope, (float) p.hiSlope, p.lowF, p.lowG, p.lowQ, (float) p.lowType, p.midF, p.midG, p.midQ, (float) p.midType, p.highF, p.highG, p.highQ, (float) p.highType,
            p.focusProc ? 1.0f : 0.0f, (float) listenOut, (float) p.msMode, p.link, p.balance, p.makeup, p.mix, p.clip ? 1.0f : 0.0f,
            (float) p.os, (float) p.window, p.sharp };
        std::copy (vals, vals + numPresetParams, v);
    }
}
