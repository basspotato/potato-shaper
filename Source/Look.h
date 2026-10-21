#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>

// Visual vocabulary shared with Potato-Kicker and Potato-Bass: orange/yellow light ramp, glossy black knobs, glowing text.
namespace look
{
using namespace juce;

namespace col
{
    // HUD palette: near-black panels with glowing orange accents
    const Colour page    { 0xff070707 };
    const Colour ink     { 0xffffb35e };   // labels
    const Colour inkDim  { 0xff9a7658 };   // values and secondary text
    const Colour amber   { 0xffff8a1e };   // primary accent
    const Colour amberHi { 0xffffc27a };   // hot highlight
    const Colour led     { 0xffff7a12 };
    const Colour ledOff  { 0xff2c1406 };
    const Colour go      { 0xffff8a1e };
    const Colour cream   { 0xffffa040 };   // lit buttons and tabs
    const Colour tick    { 0xffb3a493 };
    const Colour faceInk { 0xffff9a3c };
    const Colour faceRed { 0xffff3d1a };
    const Colour line    { 0xffff8a1e };   // tile borders and brackets (used with low alpha)
    const Colour orange  { 0xffff7212 };   // start of the orange -> yellow light ramp
    const Colour yellow  { 0xffffd23f };   // end of the ramp
}


    struct Typefaces
    {
        Typeface::Ptr semi = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
        Typeface::Ptr bold = Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
        Typeface::Ptr mono = Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
    };
    inline const Typefaces& tf() { static Typefaces t; return t; }

    // Black pebbled-vinyl case texture (sampled from a Roland RE-201 case), tiled behind the panel
    inline const Image& caseTexture()
    {
        static const Image img = ImageFileFormat::loadFrom (BinaryData::case_texture_png, BinaryData::case_texture_pngSize);
        return img;
    }

    inline Font label (float h, float kern = 0.0f, bool bold = false)
    {
        return Font (FontOptions (bold ? tf().bold : tf().semi).withHeight (h).withKerningFactor (kern));
    }
    inline Font mono (float h) { return Font (FontOptions (tf().mono).withHeight (h)); }

    inline float degToRad (float d) { return d * MathConstants<float>::pi / 180.0f; }
    inline Point<float> polar (Point<float> c, float r, float deg) { return { c.x + std::sin (degToRad (deg)) * r, c.y - std::cos (degToRad (deg)) * r }; }

    inline void drawLed (Graphics& g, Rectangle<float> r, bool on)
    {
        const auto c = r.getCentre();
        if (on)
        {
            g.setGradientFill (ColourGradient (col::led.withAlpha (0.45f), c.x, c.y, col::led.withAlpha (0.0f), c.x + r.getWidth() * 1.4f, c.y, true));
            g.fillEllipse (r.expanded (r.getWidth() * 0.9f));
            ColourGradient gr (Colour (0xfffff3c8), c.x - r.getWidth() * 0.12f, c.y - r.getHeight() * 0.15f, Colour (0xffa84300), c.x + r.getWidth() * 0.5f, c.y, true);
            gr.addColour (0.45, col::led);
            g.setGradientFill (gr);
        }
        else
        {
            g.setGradientFill (ColourGradient (Colour (0xff4a2208), c.x - r.getWidth() * 0.1f, c.y - r.getHeight() * 0.15f, col::ledOff, c.x + r.getWidth() * 0.5f, c.y, true));
        }
        g.fillEllipse (r);
        g.setColour (Colours::black);
        g.drawEllipse (r, 0.8f);
    }

    inline void glowText (Graphics& g, const String& s, Rectangle<float> r, Justification j, Colour c, float glow)
    {
        g.setColour (c.withAlpha (glow));
        for (auto d : { Point<float> (-1, 0), Point<float> (1, 0), Point<float> (0, -1), Point<float> (0, 1) })
            g.drawText (s, r.translated (d.x, d.y), j, false);
        g.setColour (c);
        g.drawText (s, r, j, false);
    }

    inline void insetWindow (Graphics& g, Rectangle<float> r)
    {
        g.setColour (col::line.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (1.0f), 4.0f);
        g.setColour (Colour (0xff080504)); g.fillRoundedRectangle (r, 3.0f);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.8f), r.getX(), r.getY(), Colours::transparentBlack, r.getX(), r.getY() + 7.0f, false));
        g.fillRoundedRectangle (r, 3.0f);
    }

    // Orange -> yellow light ramp used across the whole console (0 = orange, 1 = yellow)
    // Palette mode: 0 = orange -> yellow console light, 1 = deep blue -> deep purple -> yellow (sequencer)
    inline int& rampMode() { static int m = 0; return m; }
    // The two ends of the light ramp (message thread only). A knob can borrow its own colour pair with RampScope.
    inline Colour& rampBase() { static Colour c = col::orange; return c; }
    inline Colour& rampPeak() { static Colour c = col::yellow; return c; }
    struct RampScope
    {
        RampScope (Colour base, Colour peak) : oldBase (rampBase()), oldPeak (rampPeak()) { rampBase() = base; rampPeak() = peak; }
        ~RampScope() { rampBase() = oldBase; rampPeak() = oldPeak; }
        Colour oldBase, oldPeak;
    };
    inline Colour ramp (float t)
    {
        t = jlimit (0.0f, 1.0f, t);
        if (rampMode() == 1)
        {
            const Colour blue (0xff2b4cf0), purple (0xff8a2fe0);
            return t < 0.5f ? blue.interpolatedWith (purple, t * 2.0f) : purple.interpolatedWith (col::yellow, (t - 0.5f) * 2.0f);
        }
        return rampBase().interpolatedWith (rampPeak(), t);
    }

    // Glowing text filled with the orange -> yellow ramp from left to right
    inline void glowTextRamp (Graphics& g, const String& s, Rectangle<float> r, Justification j, float glow)
    {
        g.setColour (ramp (0.0f).withAlpha (glow));
        for (auto d : { Point<float> (-1, 0), Point<float> (1, 0), Point<float> (0, -1), Point<float> (0, 1) })
            g.drawText (s, r.translated (d.x, d.y), j, false);
        g.setGradientFill (ColourGradient (ramp (0.0f), r.getX(), 0, ramp (1.0f), r.getRight(), 0, false));
        g.drawText (s, r, j, false);
    }

    // Strokes the knob-travel arc between t0 and t1 (0..1 of the 270 degree sweep) in short segments,
    // each coloured from the ramp, so the light fades from orange at the start to yellow at the end
    inline void strokeRampArc (Graphics& g, Point<float> c, float r, float t0, float t1, float width, float alpha, float bright = 0.0f)
    {
        const int n = jmax (2, (int) ((t1 - t0) * 40.0f));
        for (int i = 0; i < n; ++i)
        {
            const float a = t0 + (t1 - t0) * (float) i / (float) n, b = t0 + (t1 - t0) * (float) (i + 1) / (float) n;
            Path seg; seg.addCentredArc (c.x, c.y, r, r, 0.0f, degToRad (-135.0f + 270.0f * a), degToRad (-135.0f + 270.0f * b), true);
            g.setColour (ramp (0.5f * (a + b)).brighter (bright).withAlpha (jlimit (0.0f, 1.0f, alpha)));
            g.strokePath (seg, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::butt));
        }
    }

    // Full ring lit with the ramp: orange at the bottom, yellow at the top
    inline void strokeRampRing (Graphics& g, Point<float> c, float r, float width, float alpha)
    {
        for (int i = 0; i < 36; ++i)
        {
            const float a = i * 10.0f, b = a + 10.0f;
            Path seg; seg.addCentredArc (c.x, c.y, r, r, 0.0f, degToRad (a), degToRad (b), true);
            g.setColour (ramp (0.5f * (1.0f + std::cos (degToRad (a + 5.0f)))).withAlpha (alpha));
            g.strokePath (seg, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::butt));
        }
    }

    // Knob: black glossy body with a backlit light-pipe ring. The lit part of the ring and the scale fade from
    // orange at the start of travel to yellow at the end, and the whole light gets brighter as the knob turns up.
    // origin: where the lit arc starts (0 = left end of travel, 0.5 = centre for bipolar controls)
    inline void drawLitKnob (Graphics& g, Point<float> c, float size, float norm, bool numbered = true, bool ticks = true, float origin = 0.0f)
    {
        const float n = jlimit (0.0f, 1.0f, norm);
        const float lo = jmin (origin, n), hi = jmax (origin, n);
        const bool any = hi - lo > 0.001f;
        auto isOn = [&] (float t) { return any && t >= lo - 0.0001f && t <= hi + 0.0001f; };
        const float k = 0.3f + 0.7f * (origin > 0.0f ? std::abs (n - origin) / jmax (origin, 1.0f - origin) : n);   // light intensity
        const float rO = size * 0.5f - 1.5f, rB = size * 0.25f;
        const float rw = size * 0.036f;
        const float rR = rB + rw * 0.5f + size * 0.04f;
        const auto box = [&] (float r) { return Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

        for (int i = 0; ticks && i <= 40; ++i)
        {
            const bool major = i % 4 == 0;
            const float t = (float) i / 40.0f;
            const bool on = isOn (t);
            const auto p1 = polar (c, major ? rO - 5.5f : rO - 3.0f, -135.0f + 270.0f * t), p2 = polar (c, rO, -135.0f + 270.0f * t);
            if (on)
            {
                g.setColour (ramp (t).withAlpha (0.35f * k)); g.drawLine (p1.x, p1.y, p2.x, p2.y, major ? 4.0f : 3.0f);
                g.setColour (ramp (t).brighter (0.35f));      g.drawLine (p1.x, p1.y, p2.x, p2.y, major ? 1.5f : 1.0f);
            }
            else
            {
                g.setColour (col::tick.withAlpha (major ? 0.75f : 0.4f)); g.drawLine (p1.x, p1.y, p2.x, p2.y, major ? 1.3f : 0.8f);
            }
        }

        const float rn = 0.5f * ((rR + rw * 0.5f + 1.2f) + (rO - 5.5f));
        g.setFont (label (size * 0.1f));
        for (int j = 0; numbered && j <= 10; ++j)
        {
            const bool on = isOn (j / 10.0f);
            g.setColour (on ? ramp (j / 10.0f).brighter (0.55f) : Colour (0xffcbbdab));
            g.drawText (String (j), Rectangle<float> (14.0f, 11.0f).withCentre (polar (c, rn, -135.0f + j * 27.0f)), Justification::centred, false);
        }

        // groove, dim backlit ring, then the lit value arc with bloom
        g.setColour (Colour (0xff040303)); g.drawEllipse (box (rR), rw + 3.0f);
        strokeRampArc (g, c, rR, 0.0f, 1.0f, rw * 0.8f, 0.10f + 0.12f * k);
        if (any)
        {
            strokeRampArc (g, c, rR, lo, hi, rw * 4.0f, 0.12f * k);
            strokeRampArc (g, c, rR, lo, hi, rw * 2.3f, 0.28f * k);
            strokeRampArc (g, c, rR, lo, hi, rw * 1.1f, 0.65f + 0.35f * k);
            strokeRampArc (g, c, rR, lo, hi, rw * 0.6f, 1.0f, 0.45f);
            const auto tip = polar (c, rR, -135.0f + 270.0f * n);
            g.setColour (ramp (n).brighter (0.6f).withAlpha (0.35f)); g.fillEllipse (Rectangle<float> (rw * 3.2f, rw * 3.2f).withCentre (tip));
            g.setColour (Colour (0xfffff4d6));                     g.fillEllipse (Rectangle<float> (rw * 1.3f, rw * 1.3f).withCentre (tip));
        }

        // light spilling onto the skirt
        g.setColour (ramp (n).withAlpha (0.12f + 0.3f * k)); g.drawEllipse (box (rB + 2.2f), 1.6f);

        // skirt, glossy body, shine
        g.setGradientFill (ColourGradient (Colour (0xff3a3632), c.x, c.y - rB - 3, Colours::black, c.x, c.y + rB + 3, false));
        g.fillEllipse (box (rB + 1.6f));
        // knurled edge (RE-201 style): fine radial grooves around the skirt
        const int knurls = size < 70.0f ? 36 : 54;
        for (int i = 0; i < knurls; ++i)
        {
            const float a = 360.0f * (float) i / (float) knurls;
            const auto k1 = polar (c, rB - 1.2f, a), k2 = polar (c, rB + 1.6f, a);
            g.setColour (Colour (0xffa59d94).withAlpha (0.42f)); g.drawLine (k1.x, k1.y, k2.x, k2.y, 0.9f);
        }
        ColourGradient body (Colour (0xff4e4a46), c.x - rB * 0.2f, c.y - rB * 0.4f, Colour (0xff020202), c.x - rB * 0.2f + rB * 1.56f, c.y - rB * 0.4f, true);
        body.addColour (0.32, Colour (0xff1d1b1a));
        g.setGradientFill (body); g.fillEllipse (box (rB - 1.0f));
        g.setColour (Colours::black); g.drawEllipse (box (rB - 1.0f), 0.8f);
        // concentric brushed rings on the cap
        for (float rr : { 0.78f, 0.56f })
        {
            g.setColour (Colours::white.withAlpha (0.07f)); g.drawEllipse (box ((rB - 1.0f) * rr), 0.8f);
        }
        const Point<float> sc (c.x - rB * 0.22f, c.y - rB * 0.4f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.24f), sc.x, sc.y, Colours::white.withAlpha (0.0f), sc.x + rB * 0.55f, sc.y, true));
        g.fillEllipse (Rectangle<float> (rB * 1.1f, rB * 0.6f).withCentre (sc));

        // pointer: lit line in the ramp colour for the current position
        const auto p1 = polar (c, rB * 0.15f, -135.0f + 270.0f * n), p2 = polar (c, rB - 3.0f, -135.0f + 270.0f * n);
        g.setColour (ramp (n).withAlpha (0.25f + 0.4f * k)); g.drawLine (p1.x, p1.y, p2.x, p2.y, 5.0f);
        g.setColour (Colour (0xfffff1d8));                   g.drawLine (p1.x, p1.y, p2.x, p2.y, jmax (2.0f, size * 0.026f));
    }


}
