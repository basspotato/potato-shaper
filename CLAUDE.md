# CLAUDE.md — Potato-Shaper

Studio transient shaper (mixing and mastering), VST3 + AU. JUCE 8.0.15, CMake, C++17. GitHub repo: `potato-shaper`.
Targets: Windows 11 x64 VST3 first; macOS 11+ Universal (VST3 + AU) is built by CI too. Keep code platform-neutral.

## Working style (token efficiency)

- Do the task. Don't narrate steps or recap finished work; end with 1-2 sentences on the outcome.
- Simple change = minimal action. Don't re-read files you just edited or add verification nobody asked for.
- Don't refactor, reformat, rename or "improve" code outside the request. No unrequested features, options, files or dependencies.
- Don't explore the repo broadly when the target file is known (see the layout below).
- Ask only when blocked on a decision that changes the outcome; otherwise pick the obvious default.
- Show a short plan before any big change. Keep DSP and GUI changes in separate sessions.
- When debugging a build error, look at only the relevant error lines.

## Git rules

- NEVER push without asking me first and getting a clear yes. Every push, every time; one approval does not carry over.
- Work on a branch and open a PR. No force-push, history rewrites or branch deletes without asking.

## Build

```bash
# Windows (Visual Studio 2022)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target PotatoShaper_VST3
# macOS
cmake -S . -B build -G Xcode && cmake --build build --config Release --target PotatoShaper_VST3 PotatoShaper_AU
# Offline DSP tests (any OS): add -DPOTATO_TESTS=ON, build PotatoShaperTests, run it. Must print ALL PASSED.
```

`-DJUCE_DIR=path/to/JUCE` uses a local JUCE checkout. CI (`.github/workflows/build.yml`) builds both platforms and runs pluginval at strictness 10.

## Source layout

```
Source/
  Params.h          all parameter IDs, ranges, value formatting
  Dsp.h             biquads, Focus curve, shared level-independent Detector, Hilbert (analytic signal)
  CleanEngine.h     zero-latency broadband engine
  SpectralEngine.h  STFT engine: per-bin detection, latency = FFT size
  PluginProcessor.* routing, M/S, side-chain, mix, makeup, warmth + safety clip (oversampled), meters, A/B
  PluginEditor.cpp  GUI: knobs, Focus display, meters
  Presets.h         90 factory presets (9 EDM groups x 10), builder syntax
  Look.h            Potato-Kicker palette, fonts, knob drawing (shared look with potato-bass / potato-kicker)
tests/OfflineTest.cpp  null, attack/tail, level-independence, steady-tone distortion, clip checks; --snapshot renders the GUI to PNG
```

## Rules for the code

- Real-time audio: no allocations, locks, or file/IO on the audio thread. Read parameters through the cached raw atomics.
- Audio quality first: run the offline tests after any DSP change; they must stay ALL PASSED.
- Keep CPU low. Flag anything that adds cost (per-sample transcendental math, bigger FFTs, higher repaint rates).
- Latency must be reported correctly (`updateLatency`); dry paths are delayed to match.
- GUI: Potato-Kicker look (black glossy knobs, orange-to-yellow light, all-caps labels, HUD tiles). Fixed 1200 x 760 panel, scaled. Build on it, don't redesign.

## Notes

- GUI snapshot (Linux): start `Xvfb :99 &`, then `DISPLAY=:99 PotatoShaperTests --snapshot out.png [spectral] [side] [bpm=N] [band=0..4] [type=0..3]`.
- Known: the macOS CI job gets cancelled at about 15 min (cause undiagnosed; Windows passes).
