#include "PluginEditor.h"
#include "Look.h"

using namespace juce;
using namespace look;

//==============================================================================
// Rotary knob (Potato-Kicker style): drag vertically, Shift for fine, double-click for default, wheel and arrow keys
class Knob : public Component
{
public:
    Knob (RangedAudioParameter& p, String title, float diameter, bool bipolar = false)
        : param (p), name (std::move (title)), size (diameter), origin (bipolar ? 0.5f : 0.0f),
          attach (p, [this] (float) { repaint(); }, nullptr)
    {
        attach.sendInitialUpdate();
        setWantsKeyboardFocus (true);
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setTitle (name);
    }

    void placeAt (int cx, int cy) { setBounds (cx - ((int) size + 40) / 2, cy - (int) size / 2 - 18, (int) size + 40, (int) size + 34); }

    void paint (Graphics& g) override
    {
        const float w = (float) getWidth();
        g.setFont (label (16.0f, 0.1f));
        glowText (g, name, Rectangle<float> (0, 0, w, 17), Justification::centred, col::ink, 0.22f);
        drawLitKnob (g, { w * 0.5f, 18.0f + size * 0.5f }, size, param.getValue(), size >= 84.0f, true, origin);
        g.setColour (dragging ? ramp (param.getValue()).brighter (0.4f) : Colour (0xffb98c62));
        g.setFont (mono (12.5f));
        g.drawText (ps::formatValue (param.getParameterID(), value()), Rectangle<float> (0, 18.0f + size, w, 15), Justification::centred, false);
        if (hasKeyboardFocus (false))
        {
            g.setColour (col::amber.withAlpha (0.6f));
            g.drawEllipse (Rectangle<float> (size * 0.66f, size * 0.66f).withCentre ({ w * 0.5f, 18.0f + size * 0.5f }), 1.5f);
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        grabKeyboardFocus();
        attach.beginGesture();
        dragging = true;
        startY = e.getScreenY();
        startN = param.getValue();
        repaint();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        const float n = startN + (float) (startY - e.getScreenY()) / (e.mods.isShiftDown() ? 900.0f : 220.0f);
        attach.setValueAsPartOfGesture (param.convertFrom0to1 (jlimit (0.0f, 1.0f, n)));
    }
    void mouseUp (const MouseEvent&) override { attach.endGesture(); dragging = false; repaint(); }
    void mouseDoubleClick (const MouseEvent&) override { attach.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (exactlyEqual (w.deltaY, 0.0f)) return;
        nudge ((w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.005f : 0.025f));
    }
    bool keyPressed (const KeyPress& k) override
    {
        const float st = k.getModifiers().isShiftDown() ? 0.005f : 0.02f;
        if (k.getKeyCode() == KeyPress::upKey || k.getKeyCode() == KeyPress::rightKey) { nudge (st); return true; }
        if (k.getKeyCode() == KeyPress::downKey || k.getKeyCode() == KeyPress::leftKey) { nudge (-st); return true; }
        return false;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    float value() const { return param.convertFrom0to1 (param.getValue()); }
    void nudge (float d) { attach.setValueAsCompleteGesture (param.convertFrom0to1 (jlimit (0.0f, 1.0f, param.getValue() + d))); }

    RangedAudioParameter& param;
    String name;
    float size, origin;
    ParameterAttachment attach;
    bool dragging = false;
    int startY = 0;
    float startN = 0.0f;
};

//==============================================================================
// Two-position bat switch with LED (Potato-Kicker)
class ToggleSwitch : public Component
{
public:
    ToggleSwitch (RangedAudioParameter& p, String title, String onText, String offText)
        : name (std::move (title)), onT (std::move (onText)), offT (std::move (offText)),
          attach (p, [this] (float v) { on = v > 0.5f; repaint(); }, nullptr)
    {
        attach.sendInitialUpdate();
        setTitle (name);
        setMouseCursor (MouseCursor::PointingHandCursor);
    }

    void placeAt (int cx, int cy) { setBounds (cx - 50, cy - 48, 100, 80); }

    void paint (Graphics& g) override
    {
        const float cx = 50.0f;
        drawLed (g, Rectangle<float> (11, 11).withCentre ({ cx, 7.5f }), on);
        const Point<float> c (cx, 38.0f);
        g.setColour (col::inkDim); g.setFont (label (13.0f, 0.08f));
        g.drawText (onT,  Rectangle<float> (0, c.y - 7, 30, 14), Justification::centredRight, false);
        g.drawText (offT, Rectangle<float> (70, c.y - 7, 30, 14), Justification::centredLeft, false);
        g.setColour (Colour (0xff0b0908)); g.fillEllipse (Rectangle<float> (30, 30).withCentre (c));
        g.setColour (col::line.withAlpha (0.35f)); g.drawEllipse (Rectangle<float> (30, 30).withCentre (c), 1.0f);
        g.setGradientFill (ColourGradient (Colour (0xff5d544c), c.x - 12, c.y - 12, Colour (0xff17130f), c.x + 12, c.y + 12, false));
        g.fillEllipse (Rectangle<float> (24, 24).withCentre (c));
        g.setColour (Colour (0xff2a2a2d)); g.fillEllipse (Rectangle<float> (12, 12).withCentre (c));
        const auto tip = polar (c, 15.0f, on ? -32.0f : 32.0f);
        g.setColour (Colour (0xffcfc6bd)); g.drawLine (c.x, c.y, tip.x, tip.y, 5.0f);
        g.setColour (on ? col::amberHi : Colour (0xffe2dad2)); g.fillEllipse (Rectangle<float> (7.2f, 7.2f).withCentre (tip));
        g.setFont (label (15.0f, 0.12f));
        glowText (g, name, Rectangle<float> (0, 60, 100, 17), Justification::centred, col::ink, 0.22f);
    }

    void mouseDown (const MouseEvent&) override { attach.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

private:
    String name, onT, offT;
    ParameterAttachment attach;
    bool on = false;
};

//==============================================================================
class SquareButton : public Button
{
public:
    explicit SquareButton (String t) : Button (t) { setTitle (t); }

    void paintButton (Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        if (down) r.translate (0, 1);
        Colour ink;
        if (getToggleState())
        {
            g.setColour (col::amber.withAlpha (0.28f)); g.fillRoundedRectangle (r.expanded (3), 5);
            g.setGradientFill (ColourGradient (col::yellow.brighter (0.2f), 0, r.getY(), col::orange, 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 3);
            ink = Colour (0xff1a0b02);
        }
        else
        {
            g.setGradientFill (ColourGradient (Colour (over ? 0xff261d16 : 0xff1d1713), 0, r.getY(), Colour (0xff0e0b09), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 3);
            g.setColour (col::line.withAlpha (over ? 0.75f : 0.45f)); g.drawRoundedRectangle (r.reduced (0.5f), 3, 1);
            ink = col::ink;
        }
        g.setColour (ink);
        g.setFont (label (14.0f, 0.06f, true));
        g.drawText (getButtonText(), r, Justification::centred, false);
    }
};

// A row of lit buttons for a choice (or bool) parameter, one button per option
class ChoiceRow : public Component
{
public:
    ChoiceRow (RangedAudioParameter& p, const StringArray& names, String title)
        : param (p), attach (p, [this] (float v) { select (roundToInt (v)); }, nullptr)
    {
        setTitle (title);
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = buttons.add (new SquareButton (names[i]));
            b->setTitle (title + ": " + names[i]);
            b->onClick = [this, i] { attach.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attach.sendInitialUpdate();
    }

    void resized() override
    {
        const int gap = 6, n = buttons.size();
        const float w = (getWidth() - gap * (n - 1)) / (float) n;
        for (int i = 0; i < n; ++i) buttons[i]->setBounds (roundToInt (i * (w + gap)), 0, roundToInt (w), getHeight());
    }

    int getSelected() const { return selected; }

private:
    void select (int i)
    {
        selected = i;
        for (int k = 0; k < buttons.size(); ++k) buttons[k]->setToggleState (k == i, dontSendNotification);
    }

    RangedAudioParameter& param;
    ParameterAttachment attach;
    OwnedArray<SquareButton> buttons;
    int selected = 0;
};

// Single lit button for a bool parameter
class ParamButton : public SquareButton
{
public:
    ParamButton (RangedAudioParameter& p, String caption)
        : SquareButton (std::move (caption)), attach (p, [this] (float v) { setToggleState (v > 0.5f, dontSendNotification); }, nullptr)
    {
        onClick = [this] { attach.setValueAsCompleteGesture (getToggleState() ? 0.0f : 1.0f); };
        attach.sendInitialUpdate();
    }

private:
    ParameterAttachment attach;
};

//==============================================================================
// Output VU (Potato-Kicker): 0 VU = -10 dBFS RMS
class VuMeter : public Component
{
public:
    void setLevel (float rms)
    {
        const float db = 20.0f * std::log10 (rms + 1.0e-6f) + 10.0f;
        level += (db - level) * (db > level ? 0.35f : 0.06f);
        repaint();
    }

    void paint (Graphics& g) override
    {
        const Point<float> pivot (125, 176);
        const float R = 118.0f;
        g.setColour (Colour (0xff060504)); g.fillRoundedRectangle (1, 1, 248, 148, 12);
        g.setColour (col::line.withAlpha (0.35f)); g.drawRoundedRectangle (1, 1, 248, 148, 12, 1.2f);
        const Rectangle<float> face (13, 13, 224, 102);
        {
            Graphics::ScopedSaveState s (g);
            Path clip; clip.addRoundedRectangle (face, 7.0f);
            g.reduceClipRegion (clip);
            g.setGradientFill (ColourGradient (Colour (0xff160d06), 0, face.getY(), Colour (0xff050302), 0, face.getBottom(), false));
            g.fillRect (face);
            g.setGradientFill (ColourGradient (Colour (0x38ff8a1e), 125, 122, Colour (0x00ff8a1e), 125 + 150, 122, true));
            g.fillRect (face);
            g.setColour (Colour (0x10ff8a1e));
            for (float x = face.getX() + 8; x < face.getRight(); x += 12) g.drawVerticalLine ((int) x, face.getY(), face.getBottom());
            for (float y = face.getY() + 6; y < face.getBottom(); y += 12) g.drawHorizontalLine ((int) y, face.getX(), face.getRight());

            Path arc;  arc.addCentredArc (pivot.x, pivot.y, R, R, 0, degToRad (ang (-20)), degToRad (ang (0)), true);
            g.setGradientFill (ColourGradient (col::orange, 20, 0, col::yellow, 230, 0, false)); g.strokePath (arc, PathStrokeType (1.6f));
            Path red;  red.addCentredArc (pivot.x, pivot.y, R, R, 0, degToRad (ang (0)), degToRad (ang (3)), true);
            g.setColour (col::faceRed); g.strokePath (red, PathStrokeType (4.0f));
            g.setFont (label (11.5f));
            for (int v : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
            {
                const float a = ang ((float) v);
                const auto p1 = polar (pivot, R - 9, a), p2 = polar (pivot, R, a), pt = polar (pivot, R + 11, a);
                g.setColour (v > 0 ? col::faceRed : ramp ((v + 20.0f) / 20.0f));
                g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.4f);
                g.drawText (v > 0 ? "+" + String (v) : String (v), Rectangle<float> (24, 12).withCentre (pt), Justification::centred, false);
            }
            g.setFont (label (13.5f, 0.0f, true));
            g.setColour (col::faceInk); g.drawText ("VU", Rectangle<float> (30, 22, 30, 14), Justification::left, false);
            g.setColour (col::faceRed); g.drawText ("VU", Rectangle<float> (190, 22, 30, 14), Justification::right, false);
            g.setColour (col::amber.withAlpha (0.45f)); g.setFont (label (9.0f, 0.25f));
            g.drawText ("POTATO SHAPER", Rectangle<float> (60, 90, 130, 12), Justification::centred, false);
            const auto tip = polar (pivot, R + 4, ang (jlimit (-20.0f, 3.5f, level)));
            g.setColour (col::amber.withAlpha (0.30f)); g.drawLine (pivot.x, pivot.y, tip.x, tip.y, 5.0f);
            g.setColour (col::yellow.brighter (0.3f)); g.drawLine (pivot.x, pivot.y, tip.x, tip.y, 1.6f);
        }
        g.setFont (label (14.5f, 0.3f));
        glowText (g, "OUTPUT", Rectangle<float> (60, 125, 130, 17), Justification::centred, col::ink, 0.22f);
        drawLed (g, Rectangle<float> (11, 11).withCentre ({ 236.5f, 126.5f }), level > 0.0f);
    }

private:
    static float ang (float v)
    {
        const float p = (std::pow (10.0f, v / 20.0f) - 0.1f) / (1.41254f - 0.1f);
        return -46.0f + 92.0f * jlimit (0.0f, 1.0f, p);
    }
    float level = -25.0f;
};

//==============================================================================
// LED bar meters: input and output peak (dBFS) plus transient and sustain gain (dB, centre = 0)
class LedMeters : public Component
{
public:
    void setLevels (const float inPk[2], const float outPk[2], float transDb, float susDb)
    {
        for (int i = 0; i < 2; ++i)
        {
            fall (lvl[i], gainToDbF (inPk[i]));
            fall (lvl[4 + i], gainToDbF (outPk[i]));
        }
        gainBar (lvl[2], transDb);
        gainBar (lvl[3], susDb);
        for (int i : { 0, 1, 4, 5 })
        {
            if (lvl[i] >= hold[i]) { hold[i] = lvl[i]; holdAge[i] = 0; }
            else if (++holdAge[i] > 45) hold[i] -= 1.0f;
        }
        repaint();
    }

    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (Colour (0xff060504)); g.fillRoundedRectangle (b.reduced (0.5f), 10);
        g.setColour (col::line.withAlpha (0.35f)); g.drawRoundedRectangle (b.reduced (0.5f), 10, 1.0f);

        static const char* names[6] = { "IN L", "IN R", "TRANS", "SUST", "OUT L", "OUT R" };
        const float colW = (b.getWidth() - 24.0f) / 6.0f, top = 26.0f, h = b.getHeight() - 52.0f;
        for (int i = 0; i < 6; ++i)
        {
            const float cx = 12.0f + colW * (i + 0.5f);
            const Rectangle<float> bar (cx - 7.0f, top, 14.0f, h);
            g.setColour (Colour (0xff1a0d05)); g.fillRect (bar);
            const bool gain = i == 2 || i == 3;
            const Colour gc = i == 2 ? Colour (0xffff4fa0) : Colour (0xff6aa8ff);
            if (gain)
            {
                const float mid = bar.getCentreY(), y = mid - jlimit (-1.0f, 1.0f, lvl[i] / 24.0f) * h * 0.5f;
                g.setColour (gc); g.fillRect (Rectangle<float> (bar.getX(), jmin (mid, y), bar.getWidth(), std::abs (y - mid)));
                g.setColour (col::inkDim); g.drawHorizontalLine ((int) mid, bar.getX() - 3, bar.getRight() + 3);
            }
            else
            {
                const float f = levelFrac (lvl[i]);
                Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (bar.withTop (bar.getBottom() - f * h).toNearestInt());
                ColourGradient cg (col::orange, 0, bar.getBottom(), col::faceRed, 0, bar.getY(), false);
                cg.addColour (1.0 - levelFrac (-6.0f), col::yellow);
                g.setGradientFill (cg); g.fillRect (bar);
            }
            // LED segment gaps
            g.setColour (Colour (0xff060504));
            for (float y = bar.getY() + 4.0f; y < bar.getBottom(); y += 6.0f) g.fillRect (bar.getX(), y, bar.getWidth(), 2.0f);
            if (! gain && hold[i] > -60.0f)
            {
                g.setColour (hold[i] > -0.1f ? col::faceRed : Colour (0xffffe2b8));
                g.fillRect (bar.getX(), bar.getBottom() - levelFrac (hold[i]) * h - 1.0f, bar.getWidth(), 2.0f);
            }

            g.setFont (mono (11.0f));
            g.setColour (gain ? gc : col::ink);
            const float v = gain ? lvl[i] : hold[i];
            const String txt = (! gain && v <= -60.0f) ? String ("-inf") : String (v, 1);
            g.drawText (txt, Rectangle<float> (cx - colW * 0.5f, 6.0f, colW, 16.0f), Justification::centred, false);
            g.setColour (col::inkDim); g.setFont (label (12.0f, 0.06f));
            g.drawText (names[i], Rectangle<float> (cx - colW * 0.5f, b.getBottom() - 22.0f, colW, 16.0f), Justification::centred, false);
        }
    }

private:
    static float gainToDbF (float g) { return g > 1.0e-6f ? 20.0f * std::log10 (g) : -120.0f; }
    static float levelFrac (float db) { return jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f); }   // -60 .. +6 dBFS
    static void fall (float& v, float target) { v = target > v ? target : jmax (target, v - 0.9f); }   // ~27 dB/s fall
    static void gainBar (float& v, float target) { v += (target - v) * (std::abs (target) > std::abs (v) ? 0.6f : 0.12f); }

    float lvl[6] { -120, -120, 0, 0, -120, -120 }, hold[6] { -120, -120, 0, 0, -120, -120 };
    int holdAge[6] {};
};

//==============================================================================
// The Focus display: live output spectrum, the Focus weighting curve with five draggable nodes, the live
// boost/cut overlay (pink = boost, blue = cut), and a scrolling detector lane (transient up, sustain down).
class FocusDisplay : public Component
{
public:
    explicit FocusDisplay (PotatoShaperProcessor& p) : proc (p)
    {
        auto get = [&] (const char* pid) { return p.apvts.getParameter (pid); };
        nodes[0] = { get (ps::id::loCut), nullptr, nullptr, "LO CUT", Colour (0xffd9dde2) };
        nodes[1] = { get (ps::id::lowF), get (ps::id::lowG), get (ps::id::lowQ), "LOW", Colour (0xff9b6bff) };
        nodes[2] = { get (ps::id::midF), get (ps::id::midG), get (ps::id::midQ), "MID", Colour (0xff6aa8ff) };
        nodes[3] = { get (ps::id::highF), get (ps::id::highG), get (ps::id::highQ), "HIGH", Colour (0xffffd23f) };
        nodes[4] = { get (ps::id::hiCut), nullptr, nullptr, "HI CUT", Colour (0xffd9dde2) };
        for (auto& s : specDb) s = -120.0f;
        setTitle ("Focus display");
    }

    void refresh (float liveGainDb, bool spectralMode)
    {
        // spectrum of the output
        proc.copyScope (fftData, fftSize, proc.getScopeWritePos());
        for (int i = 0; i < fftSize; ++i) fftData[i] *= 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) fftSize);
        std::fill (fftData + fftSize, fftData + 2 * fftSize, 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData, true);
        const double sr = proc.getRate();
        for (int x = 0; x < numPts; ++x)
        {
            const double f0 = xToFreq (x * step), f1 = xToFreq ((x + 1) * step);
            const int k0 = jlimit (1, fftSize / 2, (int) (f0 * fftSize / sr)), k1 = jlimit (k0, fftSize / 2, (int) (f1 * fftSize / sr));
            float m = 0.0f;
            for (int k = k0; k <= k1; ++k) m = jmax (m, fftData[k]);
            const float db = 20.0f * std::log10 (m * 4.0f / fftSize + 1.0e-9f);
            specDb[x] = db > specDb[x] ? db : jmax (db, specDb[x] - 1.2f);
        }

        // processing overlay
        designCurve();
        const bool focusProc = proc.apvts.getRawParameterValue (ps::id::focusProc)->load() > 0.5f;
        const auto& sd = proc.spectralDisplay();
        for (int x = 0; x < numPts; ++x)
        {
            float v;
            if (spectralMode)
            {
                const float pos = (float) x / (numPts - 1) * (ps::SpectralEngine::displayPoints - 1);
                const int i0 = (int) pos, i1 = jmin (i0 + 1, ps::SpectralEngine::displayPoints - 1);
                v = sd[(size_t) i0].load() + (sd[(size_t) i1].load() - sd[(size_t) i0].load()) * (pos - i0);
            }
            else
            {
                v = liveGainDb * (focusProc ? (float) curve.bandMagnitude (xToFreq (x * step), sr) : 1.0f);
            }
            overlay[x] += (v - overlay[x]) * (std::abs (v) > std::abs (overlay[x]) ? 0.7f : 0.2f);
        }
        repaint();
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (Colour (0xff060504)); g.fillRoundedRectangle (b.reduced (0.5f), 10);
        Graphics::ScopedSaveState clipState (g);
        Path rr; rr.addRoundedRectangle (b.reduced (1.0f), 10); g.reduceClipRegion (rr);

        // grid
        g.setFont (mono (10.0f));
        for (int fi : { 30, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 })
        {
            const float f = (float) fi, x = freqToX (f);
            const bool major = fi == 100 || fi == 1000 || fi == 10000;
            g.setColour (col::line.withAlpha (major ? 0.18f : 0.08f)); g.drawVerticalLine ((int) x, plotTop, plotBottom);
            g.setColour (col::inkDim);
            g.drawText (fi >= 1000 ? String (fi / 1000) + "k" : String (fi), Rectangle<float> (40, 12).withCentre ({ jlimit (14.f, W - 16.f, x), plotBottom + 10.f }), Justification::centred, false);
        }
        for (int d : { 12, 6, 0, -6, -12 })
        {
            const float y = gainToY ((float) d);
            g.setColour (col::line.withAlpha (d == 0 ? 0.18f : 0.07f)); g.drawHorizontalLine ((int) y, 0, W);
            g.setColour (col::inkDim); g.drawText ((d > 0 ? "+" : "") + String (d), Rectangle<float> (W - 34, y - 12, 28, 11), Justification::right, false);
        }

        // spectrum
        Path sp; sp.startNewSubPath (0, plotBottom);
        for (int x = 0; x < numPts; ++x) sp.lineTo (x * step, jlimit (plotTop, plotBottom, plotTop + (-specDb[x]) / 90.0f * (plotBottom - plotTop)));
        sp.lineTo (W, plotBottom); sp.closeSubPath();
        g.setGradientFill (ColourGradient (col::amber.withAlpha (0.22f), 0, plotTop, col::amber.withAlpha (0.02f), 0, plotBottom, false));
        g.fillPath (sp);
        g.setColour (col::amber.withAlpha (0.35f)); g.strokePath (sp, PathStrokeType (1.0f));

        // overlay: boost (pink) and cut (blue)
        for (int sign : { 1, -1 })
        {
            Path o; o.startNewSubPath (0, zeroY);
            for (int x = 0; x < numPts; ++x) o.lineTo (x * step, gainToY (sign > 0 ? jmax (0.0f, overlay[x]) : jmin (0.0f, overlay[x])));
            o.lineTo (W, zeroY); o.closeSubPath();
            const Colour c = sign > 0 ? Colour (0xffff4fa0) : Colour (0xff4f8dff);
            g.setGradientFill (ColourGradient (c.withAlpha (0.7f), 0, sign > 0 ? plotTop : plotBottom, c.withAlpha (0.12f), 0, zeroY, false));
            g.fillPath (o);
        }

        // cut regions
        const float xl = freqToX (nodeFreq (0)), xh = freqToX (nodeFreq (4));
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRect (0.0f, plotTop, xl, plotBottom - plotTop);
        g.fillRect (xh, plotTop, W - xh, plotBottom - plotTop);

        // focus curve
        const double sr = proc.getRate();
        Path cp;
        for (int x = 0; x < numPts; ++x)
        {
            const float db = (float) (20.0 * std::log10 (curve.magnitude (xToFreq (x * step), sr) + 1.0e-9));
            const float y = gainToY (jlimit (-30.0f, 18.0f, db));
            if (x == 0) cp.startNewSubPath (0, y); else cp.lineTo (x * step, y);
        }
        g.setGradientFill (ColourGradient (col::orange.withAlpha (0.35f), 0, 0, col::yellow.withAlpha (0.35f), W, 0, false));
        g.strokePath (cp, PathStrokeType (6.0f));
        g.setGradientFill (ColourGradient (col::orange, 0, 0, col::yellow, W, 0, false));
        g.strokePath (cp, PathStrokeType (2.0f));

        // nodes
        for (int i = 0; i < 5; ++i)
        {
            const auto c = nodePos (i);
            const auto& nd = nodes[i];
            const bool hot = i == hover || i == dragNode;
            g.setColour (nd.colour.withAlpha (hot ? 0.35f : 0.18f)); g.fillEllipse (Rectangle<float> (26, 26).withCentre (c));
            g.setColour (Colour (0xff0a0603)); g.fillEllipse (Rectangle<float> (14, 14).withCentre (c));
            g.setColour (nd.colour); g.drawEllipse (Rectangle<float> (14, 14).withCentre (c), 2.2f);
            g.setFont (label (12.0f, 0.08f));
            const float ly = c.y > 60.0f ? c.y - 24.0f : c.y + 12.0f;
            g.drawText (nd.name, Rectangle<float> (70, 12).withCentre ({ jlimit (30.f, W - 30.f, c.x), ly + 6.0f }), Justification::centred, false);
        }

        // readout for the node being dragged or hovered
        const int ri = dragNode >= 0 ? dragNode : hover;
        if (ri >= 0)
        {
            auto& nd = nodes[ri];
            String t = nd.name + "  " + ps::fmtHz (nodeFreq (ri));
            if (nd.gain != nullptr) t << "  " << ps::fmtDb (nd.gain->convertFrom0to1 (nd.gain->getValue()))
                                      << "  Q " << String (nd.q->convertFrom0to1 (nd.q->getValue()), 2);
            g.setFont (mono (12.0f)); g.setColour (col::yellow);
            g.drawText (t, Rectangle<float> (10, plotTop + 2, 320, 14), Justification::left, false);
        }

        // detector lane
        g.setColour (col::line.withAlpha (0.3f)); g.drawHorizontalLine ((int) laneTop, 0, W);
        const float mid = laneTop + (b.getBottom() - laneTop) * 0.5f, half = (b.getBottom() - laneTop) * 0.5f - 6.0f;
        const int lp = proc.lanePos.load (std::memory_order_acquire);
        const int cols = (int) (W / 2.0f);
        for (int i = 0; i < cols; ++i)
        {
            const int idx = (lp - cols + i + PotatoShaperProcessor::laneSize) % PotatoShaperProcessor::laneSize;
            const float m = proc.laneM[(size_t) idx].load(), s = proc.laneS[(size_t) idx].load();
            g.setColour (Colour (0xffff4fa0).withAlpha (0.85f)); g.fillRect ((float) i * 2.0f, mid - m * half, 1.6f, m * half);
            g.setColour (Colour (0xff6aa8ff).withAlpha (0.7f));  g.fillRect ((float) i * 2.0f, mid, 1.6f, s * half);
        }
        g.setFont (label (11.0f, 0.12f));
        g.setColour (col::inkDim);           g.drawText ("DETECTOR", Rectangle<float> (8, laneTop + 3, 100, 12), Justification::left, false);
        g.setColour (Colour (0xffff7ab8));   g.drawText ("TRANSIENT", Rectangle<float> (W - 108, laneTop + 3, 100, 12), Justification::right, false);
        g.setColour (Colour (0xff6aa8ff));   g.drawText ("SUSTAIN", Rectangle<float> (W - 108, b.getBottom() - 15, 100, 12), Justification::right, false);
    }

    void mouseMove (const MouseEvent& e) override { setHover (hit (e.position)); }
    void mouseExit (const MouseEvent&) override { setHover (-1); }

    void mouseDown (const MouseEvent& e) override
    {
        dragNode = hit (e.position);
        if (dragNode < 0) return;
        auto& nd = nodes[dragNode];
        widthDrag = (e.mods.isCtrlDown() || e.mods.isCommandDown()) && nd.q != nullptr;
        startY = e.position.y;
        if (widthDrag) { startQ = nd.q->getValue(); nd.q->beginChangeGesture(); }
        else
        {
            nd.freq->beginChangeGesture();
            if (nd.gain) nd.gain->beginChangeGesture();
        }
        repaint();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (dragNode < 0) return;
        auto& nd = nodes[dragNode];
        if (widthDrag)
        {
            nd.q->setValueNotifyingHost (jlimit (0.0f, 1.0f, startQ + (startY - e.position.y) / 200.0f));
            return;
        }
        set (nd.freq, xToFreq (jlimit (0.0f, W, e.position.x)));
        if (nd.gain) set (nd.gain, (zeroY - jlimit (plotTop, plotBottom, e.position.y)) / dbPx);
    }

    void mouseUp (const MouseEvent&) override
    {
        if (dragNode < 0) return;
        auto& nd = nodes[dragNode];
        if (widthDrag) nd.q->endChangeGesture();
        else
        {
            nd.freq->endChangeGesture();
            if (nd.gain) nd.gain->endChangeGesture();
        }
        dragNode = -1;
        repaint();
    }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        const int i = hit (e.position);
        if (i < 0) return;
        auto reset = [] (RangedAudioParameter* p)
        {
            if (p == nullptr) return;
            p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture();
        };
        if (nodes[i].gain) reset (nodes[i].gain); else reset (nodes[i].freq);
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const int i = hit (e.position);
        if (i < 0 || nodes[i].q == nullptr || exactlyEqual (w.deltaY, 0.0f)) return;
        auto* q = nodes[i].q;
        q->beginChangeGesture();
        q->setValueNotifyingHost (jlimit (0.0f, 1.0f, q->getValue() + (w.deltaY > 0 ? 0.03f : -0.03f)));
        q->endChangeGesture();
    }

private:
    struct Node { RangedAudioParameter *freq = nullptr, *gain = nullptr, *q = nullptr; String name; Colour colour; };

    static constexpr float W = 600.0f, plotTop = 8.0f, plotBottom = 282.0f, laneTop = 304.0f;
    static constexpr float zeroY = 0.5f * (plotTop + plotBottom), dbPx = (plotBottom - plotTop) * 0.5f / 18.0f;
    static constexpr int numPts = 300;
    static constexpr float step = W / (numPts - 1);
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;

    static float freqToX (float f) { return W * std::log (f / 20.0f) / std::log (1000.0f); }
    static float xToFreq (float x) { return 20.0f * std::pow (1000.0f, x / W); }
    static float gainToY (float db) { return zeroY - db * dbPx; }
    static void set (RangedAudioParameter* p, float v) { p->setValueNotifyingHost (p->convertTo0to1 (v)); }

    float nodeFreq (int i) const { return nodes[i].freq->convertFrom0to1 (nodes[i].freq->getValue()); }
    Point<float> nodePos (int i) const
    {
        const float g = nodes[i].gain ? nodes[i].gain->convertFrom0to1 (nodes[i].gain->getValue()) : 0.0f;
        return { jlimit (8.0f, W - 8.0f, freqToX (nodeFreq (i))), gainToY (g) };
    }
    int hit (Point<float> p) const
    {
        int best = -1; float bd = 16.0f;
        for (int i = 0; i < 5; ++i) { const float d = nodePos (i).getDistanceFrom (p); if (d < bd) { bd = d; best = i; } }
        return best;
    }
    void setHover (int i) { if (i != hover) { hover = i; setMouseCursor (i >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor); repaint(); } }

    void designCurve()
    {
        auto v = [&] (const char* pid) { return proc.apvts.getRawParameterValue (pid)->load(); };
        using namespace ps::id;
        ps::FocusSettings s { v (loCut), v (hiCut), v (lowF), v (lowG), v (lowQ), v (midF), v (midG), v (midQ), v (highF), v (highG), v (highQ) };
        curve.design (s, proc.getRate());
    }

    PotatoShaperProcessor& proc;
    Node nodes[5];
    ps::FocusCurve curve;
    dsp::FFT fft { fftOrder };
    float fftData[2 * fftSize] {};
    float specDb[numPts] {}, overlay[numPts] {};
    int hover = -1, dragNode = -1;
    bool widthDrag = false;
    float startY = 0, startQ = 0;
};

//==============================================================================
// Potato mascot badge (Potato-Kicker)
class PotatoBadge : public Component
{
public:
    void setActivity (float a)
    {
        a = jlimit (0.0f, 1.0f, a);
        if (std::abs (a - act) > 0.01f) { act = a; repaint(); }
    }

    void paint (Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const Point<float> c (b.getCentreX(), b.getCentreY());
        const float R = jmin (b.getWidth(), b.getHeight()) * 0.37f;
        const Path spud = potatoPath (c, R);
        g.setGradientFill (ColourGradient (col::amber.withAlpha (0.16f + 0.14f * act), c.x, c.y, col::amber.withAlpha (0.0f), c.x + R * 1.7f, c.y, true));
        g.fillEllipse (Rectangle<float> (R * 3.4f, R * 2.7f).withCentre (c));
        g.setGradientFill (ColourGradient (Colour (0xff3a200c), c.x - R * 0.35f, c.y - R * 0.45f, Colour (0xff0d0704), c.x + R * 0.95f, c.y + R * 0.8f, true));
        g.fillPath (spud);
        const auto sb = spud.getBounds();
        auto rampFill = [&] (float a) { return ColourGradient (col::orange.withAlpha (a), sb.getX(), sb.getBottom(), col::yellow.withAlpha (a), sb.getRight(), sb.getY(), false); };
        g.setGradientFill (rampFill (0.16f)); g.strokePath (spud, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setGradientFill (rampFill (1.0f));  g.strokePath (spud, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
        for (int i = 0; i < 2; ++i)
        {
            const Point<float> e = c + Point<float> (i == 0 ? -0.34f : 0.30f, i == 0 ? -0.16f : -0.19f) * R;
            g.setColour (Colour (0xff080402)); g.fillEllipse (Rectangle<float> (R * 0.30f, R * 0.36f).withCentre (e));
            const float pr = R * (0.075f + 0.03f * act);
            g.setColour (col::yellow); g.fillEllipse (Rectangle<float> (pr * 2.0f, pr * 2.0f).withCentre (e));
        }
        Path mouth;
        mouth.startNewSubPath (c.x - R * 0.22f, c.y + R * 0.30f);
        mouth.quadraticTo (c.x - R * 0.02f, c.y + R * 0.46f, c.x + R * 0.22f, c.y + R * 0.27f);
        g.setColour (col::yellow); g.strokePath (mouth, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }

private:
    static Path potatoPath (Point<float> c, float R)
    {
        const float radii[16] { 1.02f, 0.97f, 0.90f, 0.93f, 0.99f, 0.95f, 0.91f, 0.98f, 1.06f, 1.03f, 0.94f, 0.90f, 0.96f, 1.04f, 1.00f, 1.05f };
        const float tilt = degToRad (-10.0f), ct = std::cos (tilt), st = std::sin (tilt);
        Point<float> pts[16];
        for (int i = 0; i < 16; ++i)
        {
            const float a = degToRad (i * 22.5f);
            const float x = std::cos (a) * R * 1.22f * radii[i], y = std::sin (a) * R * radii[i];
            pts[i] = c + Point<float> (x * ct - y * st, x * st + y * ct);
        }
        Path p;
        auto mid = [&] (int i, int j) { return (pts[i] + pts[j]) * 0.5f; };
        p.startNewSubPath (mid (15, 0));
        for (int i = 0; i < 16; ++i) p.quadraticTo (pts[i], mid (i, (i + 1) % 16));
        p.closeSubPath();
        return p;
    }

    float act = 0.0f;
};

//==============================================================================
class PanelContent : public Component
{
public:
    explicit PanelContent (PotatoShaperProcessor& p)
        : proc (p), display (p),
          mode   (*p.apvts.getParameter (ps::id::mode), ps::modeNames, "Engine"),
          osRow  (*p.apvts.getParameter (ps::id::os), ps::osNames, "Oversampling"),
          msRow  (*p.apvts.getParameter (ps::id::msMode), StringArray { "L / R", "M / S" }, "Stereo mode"),
          scRow  (*p.apvts.getParameter (ps::id::extSc), StringArray { "INTERNAL", "EXTERNAL" }, "Side-chain source"),
          winRow (*p.apvts.getParameter (ps::id::window), ps::windowNames, "Spectral window"),
          listen (*p.apvts.getParameter (ps::id::listen), ps::listenNames, "Listen"),
          clipBtn (*p.apvts.getParameter (ps::id::clip), "SAFETY CLIP"),
          bypassBtn (*p.apvts.getParameter (ps::id::bypass), "BYPASS"),
          focusSw (*p.apvts.getParameter (ps::id::focusProc), "FOCUS", "PROC", "DET")
    {
        auto& s = p.apvts;
        auto knob = [&] (const char* pid, const char* title, float size, int x, int y, bool bipolar = false)
        {
            auto* k = knobs.add (new Knob (*s.getParameter (pid), title, size, bipolar));
            addAndMakeVisible (k);
            k->placeAt (x, y);
        };
        knob (ps::id::trans,   "TRANSIENT", 108, 395, 562, true);
        knob (ps::id::sus,     "SUSTAIN",   108, 530, 562, true);
        knob (ps::id::time,    "TIME",       92, 660, 562);
        knob (ps::id::warmth,  "WARMTH",     92, 790, 562);
        knob (ps::id::makeup,  "MAKEUP",     88, 990, 562);
        knob (ps::id::mix,     "MIX",        88, 1110, 562);
        knob (ps::id::link,    "LINK",       72, 85, 198);
        knob (ps::id::balance, "BALANCE",    72, 215, 198, true);
        knob (ps::id::scGain,  "SC GAIN",    72, 85, 424, true);
        knob (ps::id::sharp,   "SHARPNESS",  64, 150, 606);

        for (Component* c : { (Component*) &display, (Component*) &mode, (Component*) &osRow, (Component*) &msRow, (Component*) &scRow,
                              (Component*) &winRow, (Component*) &listen, (Component*) &clipBtn, (Component*) &bypassBtn,
                              (Component*) &focusSw, (Component*) &vu, (Component*) &meters, (Component*) &badge,
                              (Component*) &abA, (Component*) &abB, (Component*) &abCopy })
            addAndMakeVisible (c);

        badge.setBounds (14, 4, 56, 50);
        mode.setBounds (330, 16, 220, 30);
        abA.setBounds (590, 16, 40, 30); abB.setBounds (636, 16, 40, 30); abCopy.setBounds (682, 16, 70, 30);
        osRow.setBounds (1010, 16, 170, 30);
        display.setBounds (300, 64, 600, 380);
        vu.setBounds (926, 64, 250, 150);
        meters.setBounds (926, 226, 250, 218);
        msRow.setBounds (24, 98, 252, 30);
        scRow.setBounds (24, 324, 252, 30);
        focusSw.placeAt (215, 440);
        listen.setBounds (110, 690, 520, 36);
        clipBtn.setBounds (930, 690, 130, 36);
        bypassBtn.setBounds (1072, 690, 106, 36);
        winRow.setBounds (24, 518, 252, 30);

        abA.setClickingTogglesState (false); abB.setClickingTogglesState (false);
        abA.onClick = [this] { proc.selectAB (0); syncAB(); };
        abB.onClick = [this] { proc.selectAB (1); syncAB(); };
        abCopy.onClick = [this] { proc.copyABToOther(); };
        abCopy.setTitle (String ("Copy to other A/B slot"));
        syncAB();
    }

    void paint (Graphics& g) override
    {
        g.fillAll (col::page);
        g.setFont (label (27.0f, 0.12f, true));
        glowTextRamp (g, "POTATO-SHAPER", Rectangle<float> (76, 12, 240, 32), Justification::left, 0.3f);
        g.setFont (label (13.0f, 0.2f)); g.setColour (col::inkDim);
        g.drawText ("ENGINE", Rectangle<float> (330, 0, 220, 14), Justification::left, false);
        g.drawText ("A / B", Rectangle<float> (590, 0, 160, 14), Justification::left, false);
        g.drawText ("OVERSAMPLE", Rectangle<float> (1010, 0, 170, 14), Justification::left, false);

        auto section = [&] (const char* name, int y)
        {
            g.setFont (label (14.0f, 0.25f)); g.setColour (col::inkDim);
            g.drawText (name, Rectangle<float> (24, (float) y, 252, 18), Justification::left, false);
            g.setColour (col::line.withAlpha (0.25f)); g.drawHorizontalLine (y + 20, 24, 276);
        };
        section ("STEREO", 70);
        section ("SIDE-CHAIN", 296);
        section (spectral ? "SPECTRAL" : "SPECTRAL  (ENGINE OFF)", 490);

        g.setFont (label (12.5f, 0.15f)); g.setColour (col::inkDim);
        g.drawText ("FOCUS: DRAG NODES   CTRL-DRAG OR WHEEL = WIDTH   DOUBLE-CLICK = RESET", Rectangle<float> (300, 448, 600, 16), Justification::centred, false);

        g.setColour (col::line.withAlpha (0.25f)); g.drawHorizontalLine (664, 20, 1180);
        g.setFont (label (14.0f, 0.25f)); g.setColour (col::inkDim);
        g.drawText ("LISTEN", Rectangle<float> (24, 690, 80, 36), Justification::left, false);
        g.setFont (mono (12.0f));
        g.drawText (latencyMs > 0.0f ? "LATENCY " + String (latencyMs, 1) + " ms" : String ("ZERO LATENCY"),
                    Rectangle<float> (650, 690, 200, 36), Justification::centredLeft, false);
        drawLed (g, Rectangle<float> (11, 11).withCentre ({ 918.0f, 708.0f }), clipLit > 0);
    }

    void refresh()
    {
        float inPk[2], outPk[2];
        for (int i = 0; i < 2; ++i)
        {
            inPk[i] = proc.inPeak[i].exchange (0.0f);
            outPk[i] = proc.outPeak[i].exchange (0.0f);
        }
        const float t = proc.transDb.exchange (0.0f), s = proc.susDb.exchange (0.0f);
        meters.setLevels (inPk, outPk, t, s);
        vu.setLevel (proc.outRms.load());
        badge.setActivity (jmax (outPk[0], outPk[1]));

        const bool spec = proc.apvts.getRawParameterValue (ps::id::mode)->load() > 0.5f;
        display.refresh (t + s, spec);

        const float lat = 1000.0f * (float) proc.getLatencySamples() / (float) jmax (1.0, proc.getRate());
        int clip = clipLit;
        if (proc.clipped.exchange (false)) clip = 15; else if (clip > 0) --clip;
        if (spec != spectral || std::abs (lat - latencyMs) > 0.05f || clip != clipLit)
        {
            spectral = spec; latencyMs = lat; clipLit = clip;
            repaint();
        }
    }

private:
    void syncAB()
    {
        abA.setToggleState (proc.getABSlot() == 0, dontSendNotification);
        abB.setToggleState (proc.getABSlot() == 1, dontSendNotification);
    }

    PotatoShaperProcessor& proc;
    FocusDisplay display;
    OwnedArray<Knob> knobs;
    ChoiceRow mode, osRow, msRow, scRow, winRow, listen;
    ParamButton clipBtn, bypassBtn;
    ToggleSwitch focusSw;
    VuMeter vu;
    LedMeters meters;
    PotatoBadge badge;
    SquareButton abA { "A" }, abB { "B" }, abCopy { "COPY" };
    bool spectral = false;
    float latencyMs = 0.0f;
    int clipLit = 0;
};

//==============================================================================
PotatoShaperEditor::PotatoShaperEditor (PotatoShaperProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    panel = std::make_unique<PanelContent> (p);
    addAndMakeVisible (*panel);
    panel->setBounds (0, 0, panelW, panelH);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) panelW / panelH);
        c->setSizeLimits (panelW * 6 / 10, panelH * 6 / 10, panelW * 3 / 2, panelH * 3 / 2);
    }
    setSize (panelW * 9 / 10, panelH * 9 / 10);
    startTimerHz (30);
}

PotatoShaperEditor::~PotatoShaperEditor() { stopTimer(); }

void PotatoShaperEditor::paint (Graphics& g) { g.fillAll (col::page); }

void PotatoShaperEditor::resized()
{
    panel->setTransform (AffineTransform::scale ((float) getWidth() / (float) panelW));
}

void PotatoShaperEditor::timerCallback() { panel->refresh(); }
