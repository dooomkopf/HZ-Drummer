// The symbols behind the text of some buttons (count-in, start/pause, tap, mixer, system) and the arranger's
// play icon, drawn by HZLook. Moved here from HZLook.cpp unchanged.

#include "app/views/ButtonSymbols.h"

#include <cmath>
#include <vector>

namespace
{
    // Vertical ranges in which a vertical stroke at x comes closer than 'gap' to the letters: the glyphs
    // dilated by 'gap', sampled along the stroke only (just the one or two glyphs it passes).
    std::vector<juce::Range<float>> rangesNearLetters (const juce::GlyphArrangement& text, float x, float halfWidth, float gap)
    {
        const float reach = halfWidth + gap;
        juce::Path letters;
        for (int i = 0; i < text.getNumGlyphs(); ++i)
            if (const auto& glyph = text.getGlyph (i); glyph.getRight() > x - reach && glyph.getLeft() < x + reach)
                glyph.createPath (letters);

        std::vector<juce::Range<float>> ranges;
        if (letters.isEmpty())
            return ranges;

        constexpr float step = 0.25f;
        const auto box = letters.getBounds();
        for (float y = box.getY(); y <= box.getBottom(); y += step)
        {
            bool ink = false;
            for (int k = -2; k <= 2 && ! ink; ++k)
                ink = letters.contains (x + 0.5f * reach * (float) k, y);
            if (! ink)
                continue;
            const juce::Range<float> near (y - gap, y + gap);
            if (! ranges.empty() && ranges.back().getEnd() >= near.getStart())
                ranges.back() = ranges.back().getUnionWith (near);
            else
                ranges.push_back (near);
        }
        return ranges;
    }
}

namespace buttonsymbols
{

    // The count-in button's background (the user's draft, variant C): a large stylised metronome
    // on the right, cut off by the pill, behind the centred text. Coordinates in units of the pill
    // height, x from the pill's right edge, y from its top, so it scales cleanly with size and DPI.
    void drawMetronome (juce::Graphics& g, juce::Rectangle<float> pill, float opacity)
    {
        const float h = pill.getHeight();
        auto at = [&] (float x, float y) { return juce::Point<float> (pill.getRight() + x * h, pill.getY() + y * h); };

        // housing sides, running past the top and bottom edge
        constexpr float leftTop = -0.571f, leftBottom = -0.830f, rightTop = -0.323f, rightBottom = -0.153f;
        auto left  = [] (float y) { return leftTop + (leftBottom - leftTop) * y; };
        auto right = [] (float y) { return rightTop + (rightBottom - rightTop) * y; };

        // pendulum: pivot exactly midway between the sides, leaning to the upper right
        constexpr float pivotY = 0.941f, lean = 0.514f, tipY = 0.18f, weightY = 0.3215f;
        const float pivotX = 0.5f * (left (pivotY) + right (pivotY));
        auto rodX = [&] (float y) { return pivotX + lean * (pivotY - y); };

        // the right side pauses where the pendulum and its weight pass it
        constexpr float gapFrom = 0.313f, gapTo = 0.485f, beyond = 0.05f;
        juce::Path housing;
        housing.startNewSubPath (at (left (-beyond), -beyond));
        housing.lineTo (at (left (1.0f + beyond), 1.0f + beyond));
        housing.startNewSubPath (at (right (-beyond), -beyond));
        housing.lineTo (at (right (gapFrom), gapFrom));
        housing.startNewSubPath (at (right (gapTo), gapTo));
        housing.lineTo (at (right (1.0f + beyond), 1.0f + beyond));

        g.setColour (juce::Colours::white.withAlpha (0.30f * opacity));
        g.strokePath (housing, juce::PathStrokeType (juce::jmax (1.0f, 0.028f * h)));

        juce::Path rod;
        rod.startNewSubPath (at (rodX (1.0f + beyond), 1.0f + beyond));
        rod.lineTo (at (rodX (tipY), tipY));
        g.strokePath (rod, juce::PathStrokeType (juce::jmax (1.0f, 0.033f * h)));

        // the small weight, upright on the rod
        juce::Path weight;
        weight.addRectangle (-0.045f * h, -0.0615f * h, 0.09f * h, 0.123f * h);
        weight.applyTransform (juce::AffineTransform::rotation (std::atan (lean)).translated (at (rodX (weightY), weightY)));
        g.fillPath (weight);
    }

    // Transport draft A: fine strokes at the right edge, clipped by the pill like the metronome.
    void drawTransport (juce::Graphics& g, juce::Rectangle<float> pill, bool playing, float opacity)
    {
        const float h = pill.getHeight();
        auto at = [&] (float x, float y) { return juce::Point<float> (pill.getRight() + x * h, pill.getY() + y * h); };

        juce::Path symbol;
        if (playing)
        {
            for (const float x : { -0.63f, -0.34f })
            {
                symbol.startNewSubPath (at (x, -0.08f));
                symbol.lineTo (at (x, 1.08f));
            }
        }
        else
        {
            symbol.startNewSubPath (at (-0.64f, -0.08f));
            symbol.lineTo (at (-0.14f, 0.5f));
            symbol.lineTo (at (-0.64f, 1.08f));
        }

        g.setColour (juce::Colours::white.withAlpha (0.30f * opacity));
        g.strokePath (symbol, juce::PathStrokeType (juce::jmax (1.0f, 0.028f * h)));
    }

    // Tap draft B (docs/entwuerfe/tap-B.svg): a three-quarter circle at the right edge, cut off at top
    // and bottom, open towards the "p", with a speedometer needle to the upper right.
    void drawTap (juce::Graphics& g, juce::Rectangle<float> pill, float opacity)
    {
        const float h = pill.getHeight();
        const float cx = pill.getRight() - 0.675f * h;
        const float cy = pill.getY() + 0.5f * h;
        const float radius = 0.5625f * h;
        const float needle = 0.475f * h / juce::MathConstants<float>::sqrt2;

        juce::Path symbol;
        // angles clockwise from 12 o'clock: upper left over the right to lower left (270 degrees)
        symbol.addCentredArc (cx, cy, radius, radius, 0.0f,
                              -juce::MathConstants<float>::pi / 4.0f, 5.0f * juce::MathConstants<float>::pi / 4.0f, true);
        symbol.startNewSubPath (cx, cy);
        symbol.lineTo (cx + needle, cy - needle);

        g.setColour (juce::Colours::white.withAlpha (0.30f * opacity));
        g.strokePath (symbol, juce::PathStrokeType (juce::jmax (1.0f, 0.028f * h)));
    }

    // The transport strokes as a centred icon instead of text (the arranger's Play/Stopp), in the current
    // colour: > to play, || while something plays
    void drawTransportIcon (juce::Graphics& g, juce::Rectangle<float> pill, bool playing)
    {
        const float h = pill.getHeight();
        const float s = 0.5f * h;   // icon height
        const auto c = pill.getCentre();
        juce::Path symbol;
        if (playing)
        {
            for (const float dx : { -0.15f * s, 0.15f * s })
            {
                symbol.startNewSubPath (c.x + dx, c.y - 0.5f * s);
                symbol.lineTo (c.x + dx, c.y + 0.5f * s);
            }
        }
        else
        {
            symbol.startNewSubPath (c.x - 0.2f * s, c.y - 0.5f * s);
            symbol.lineTo (c.x + 0.23f * s, c.y);
            symbol.lineTo (c.x - 0.2f * s, c.y + 0.5f * s);
        }
        g.strokePath (symbol, juce::PathStrokeType (juce::jmax (1.5f, 0.07f * h), juce::PathStrokeType::mitered,
                                                    juce::PathStrokeType::rounded));
    }

    // Mixer draft B, lines closer (docs/entwuerfe/mixer-B.svg): four faders at the right edge,
    // cut off at the bottom, the third knob a little lower. The two left faders run through the
    // text and make way for the letters: the first with level cuts, the second wedge-shaped
    // (lower edge rising at 30 degrees, upper edge falling at 45 degrees).
    void drawMixer (juce::Graphics& g, juce::Rectangle<float> pill, float opacity, const juce::GlyphArrangement& text)
    {
        const float h = pill.getHeight();
        const float stroke = juce::jmax (1.0f, 0.028f * h);
        const float dotRadius = 0.035f * h;
        juce::Path lines, dots, cuts;
        for (int i = 0; i < 4; ++i)
        {
            const float x = pill.getRight() + (-0.91f + 0.20f * (float) i) * h;
            const float y = pill.getY() + (i == 2 ? 0.27f : 0.20f) * h;
            lines.startNewSubPath (x, y);
            lines.lineTo (x, pill.getY() + 1.08f * h);
            dots.addEllipse (x - dotRadius, y - dotRadius, 2.0f * dotRadius, 2.0f * dotRadius);

            if (i < 2)
            {
                const float gap = (i == 0 ? 0.10f : 0.06f) * h;   // clearance between fader and letters
                for (const auto r : rangesNearLetters (text, x, 0.5f * stroke, gap))
                {
                    // a cut a little wider than the stroke; slopes as height change over half its width
                    const bool wedge = i == 1 && r.getLength() > 2.0f * stroke;
                    const float upper = wedge ? stroke * std::tan (juce::MathConstants<float>::pi / 4.0f) : 0.0f;
                    const float lower = wedge ? stroke * std::tan (juce::MathConstants<float>::pi / 6.0f) : 0.0f;
                    cuts.addQuadrilateral (x - stroke, r.getStart() - upper, x + stroke, r.getStart() + upper,
                                           x + stroke, r.getEnd() - lower,   x - stroke, r.getEnd() + lower);
                }
            }
        }

        if (! cuts.isEmpty())
        {
            // even-odd: everything around the pill except the cuts
            juce::Path outside (cuts);
            outside.addRectangle (pill.expanded (2.0f));
            outside.setUsingNonZeroWinding (false);
            g.reduceClipRegion (outside);
        }

        // one transparency layer, so line and knob do not add up to a brighter spot where they overlap
        g.beginTransparencyLayer (0.30f * opacity);
        g.setColour (juce::Colours::white);
        g.strokePath (lines, juce::PathStrokeType (stroke));
        g.fillPath (dots);
        g.endTransparencyLayer();
    }

    // System draft A (docs/entwuerfe/system-A.svg): a three-quarter circle like Tap, smaller, open to
    // the left, its radius modulated by a flat sine wave into a subtle gear outline.
    void drawSystem (juce::Graphics& g, juce::Rectangle<float> pill, float opacity)
    {
        const float h = pill.getHeight();
        const float cx = pill.getRight() - 0.55f * h;
        const float cy = pill.getY() + 0.5f * h;
        const float pi = juce::MathConstants<float>::pi;
        constexpr int segments = 432;

        // angles from the positive x axis, clockwise on screen: upper left over the right to lower left
        juce::Path contour;
        for (int i = 0; i <= segments; ++i)
        {
            const float angle = -0.75f * pi + 1.5f * pi * (float) i / (float) segments;
            const float radius = h * (0.4375f + 0.0175f * std::sin (12.0f * angle + 0.5f * pi));
            const float x = cx + radius * std::cos (angle);
            const float y = cy + radius * std::sin (angle);
            if (i == 0)
                contour.startNewSubPath (x, y);
            else
                contour.lineTo (x, y);
        }

        g.setColour (juce::Colours::white.withAlpha (0.30f * opacity));
        g.strokePath (contour, juce::PathStrokeType (juce::jmax (1.0f, 0.028f * h), juce::PathStrokeType::curved));
    }

}
