# Potato-Shaper

A transient shaper for mixing and mastering, in VST3 (Windows, macOS) and AU (macOS) formats.

It has two engines:

- **Clean** has zero latency. It uses broadband processing driven by a level-independent detector.
- **Spectral** is frequency-selective. Each FFT bin gets its own detector, so only the frequencies that carry the transient, or the sustain, are boosted or cut. Its latency is the FFT size: 10.7, 21.3 or 42.7 ms at 48 kHz for the Short, Medium and Long windows.

Detection is level independent, so there is no threshold. Turning the track up or down doesn't change what gets shaped.

## Controls

| Control | What it does |
| --- | --- |
| Transient | Gain of the detected attacks (±24 dB). In M/S mode, the MID and SIDE tabs above the Transient and Sustain knobs choose which channel the two knobs edit, so the Mid and Side get their own settings. |
| Sustain | Gain of the detected tails (±24 dB). |
| Time | Length of the transient region (2 to 200 ms). A longer Time also starts the sustain later. |
| Warmth | Saturation applied to the transients only. |
| Focus display | Drag the five nodes (Lo Cut, Low, Mid, High, Hi Cut) to weight what the detector reacts to. Click a node to open its box, with Freq, Sens and Q knobs. Low, Mid and High have a shape selector (low shelf, peak, high shelf, tilt). Lo Cut and Hi Cut have a slope selector (HP or LP at 6, 12, 24 or 48 dB/oct), where Sens is greyed out and Q sets the corner resonance (greyed out at 6 dB/oct). Ctrl-drag or the mouse wheel changes width, and double-click resets a node. Click an empty spot to close the box. |
| Presets | 90 EDM factory presets in 9 groups (EDM, Bass, Synth, Kick, Hat, Clap, Vocals, Imaging, Master). Arrows step through them; click the name to open the browser. A dot after the name means it was edited. |
| Focus switch | **DET** shapes the whole signal. **PROC** also limits processing to the band between Lo Cut and Hi Cut. |
| Listen | Out, Transient, Sustain, Delta (what the plugin changes) or Sidechain (what the detector hears). |
| Stereo | L/R or M/S processing, Link (0 to 100 %), and Balance (how much each channel is processed). |
| Side-chain | Internal or External detection, with SC Gain. |
| Makeup, Mix | Output gain and dry/wet blend. The dry signal is latency-aligned. |
| Safety Clip | Hard-clips the output at 0 dBFS. |
| Oversample | Off, 2x or 4x for the Warmth and Clip stage. It uses linear-phase FIR filters and adds a little latency. |
| Spectral | Window (Short, Medium, Long) and Sharpness (how narrow the processed bands can be). |
| A / B | Compares two settings. Copy sends the current settings to the other slot. |

The display shows the live output spectrum, the Focus curve, the live boost (pink) and cut (blue) per frequency, and a scrolling detector lane. Meters show input, output, transient gain, sustain gain and an output VU.

## Building on Windows

You need:

- Visual Studio 2022 with the **Desktop development with C++** workload
- CMake 3.22 or newer (Visual Studio's bundled CMake works)
- Git

Then run:

```powershell
git clone https://github.com/basspotato/potato-shaper.git
cd potato-shaper
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target PotatoShaper_VST3
```

The first configure downloads JUCE 8.0.15. To use a checkout you already have, add `-DJUCE_DIR=C:\path\to\JUCE` instead.

The plugin is built at `build\PotatoShaper_artefacts\Release\VST3\Potato-Shaper.vst3`. To install it, copy that folder to `C:\Program Files\Common Files\VST3\` and rescan plugins in your DAW.

You can also skip building. Every push and PR is built by GitHub Actions, which also runs pluginval at strictness 10. Download the `Potato-Shaper-VST3-Windows-x64` artifact from the run page.

## Building on macOS

```bash
cmake -S . -B build -G Xcode
cmake --build build --config Release --target PotatoShaper_VST3 PotatoShaper_AU
```

## Tests

```bash
cmake -S . -B build -DPOTATO_TESTS=ON
cmake --build build --config Release --target PotatoShaperTests
```

Then run `PotatoShaperTests` from `build/PotatoShaperTests_artefacts/Release/`. It checks both engines:

- neutral settings null against the input
- transient and sustain moves land on the attack and the tail
- results don't change when the input level changes
- a steady tone isn't distorted
- Safety Clip holds the output at 0 dBFS

`PotatoShaperTests --snapshot out.png [spectral]` renders the GUI to an image.

## Credits

Fonts: Barlow Condensed and Share Tech Mono, both under the SIL Open Font License (see `Resources/Fonts`). The GUI look is shared with Potato-Kicker.
