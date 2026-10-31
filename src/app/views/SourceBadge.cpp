#include "app/views/SourceBadge.h"
#include "app/views/MaterialBadgePaths.h"

namespace theme
{

juce::Colour partColour (int index)
{
    static const juce::Colour colours[numPartColours] = {
        juce::Colour (0xff1178a5), juce::Colour (0xff2043a3), juce::Colour (0xff5f5fc8), juce::Colour (0xff673672),
        juce::Colour (0xff9b47a1), juce::Colour (0xffb73c59), juce::Colour (0xff900e26), juce::Colour (0xff935415),
        juce::Colour (0xffb79019), juce::Colour (0xff5e7b16), juce::Colour (0xff095e19), juce::Colour (0xff55565c) };
    if (index == restColourIndex)  return juce::Colour (0xff2e3440);
    if (index == clickColourIndex) return juce::Colour (0xff7a7a84);
    return colours[(size_t) (index < 0 ? 0 : index) % numPartColours];
}

void drawSourceBadge (juce::Graphics& g, juce::Rectangle<float> r, int colourIndex, int shapeIndex)
{
    const juce::Colour grey (0xffd8d8dc);
    const float corner = juce::jmin (3.0f, r.getWidth() * 0.25f);
    // a dark rim first, so the badge stands out even on a block of its own colour
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.expanded (1.5f), corner + 1.0f);
    g.setColour (partColour (colourIndex));
    g.fillRoundedRectangle (r, corner);

    const int shape = ((shapeIndex % numBadgeShapes) + numBadgeShapes) % numBadgeShapes;

    if (! isOwnMark (shape))   // a Material Symbol, filled in the same grey as the marks, without outline
    {
        // read on first use: hundreds of outlines, of which a window shows only a few
        static std::vector<std::unique_ptr<juce::Path>> paths (materialbadges::svgPaths.size());
        auto& path = paths[(size_t) (shape - numOwnMarks)];
        if (path == nullptr)
            path = std::make_unique<juce::Path> (juce::Drawable::parseSVGPath (materialbadges::svgPaths[(size_t) (shape - numOwnMarks)]));
        const auto inner = r.reduced (r.getWidth() * 0.04f);
        const auto transform = juce::AffineTransform::translation (0.0f, 960.0f)   // viewBox 0 -960 960 960
            .scaled (inner.getWidth() / 960.0f, inner.getHeight() / 960.0f)
            .translated (inner.getX(), inner.getY());
        g.setColour (grey);
        g.fillPath (*path, transform);
        return;
    }

    juce::Path mark;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();

    switch (shape)
    {
        case 0: break;                                                       // plain
        case 1: mark.addRectangle (x, y, w, h * 0.5f); break;                // top half
        case 2: mark.addRectangle (x, y + h * 0.5f, w, h * 0.5f); break;     // bottom half
        case 3: mark.addRectangle (x, y, w * 0.5f, h); break;                // left half
        case 4: mark.addRectangle (x + w * 0.5f, y, w * 0.5f, h); break;     // right half
        case 5: mark.addTriangle (x, y, x + w, y, x, y + h); break;          // upper-left triangle
        case 6: mark.addTriangle (x + w, y, x + w, y + h, x, y + h); break;  // lower-right triangle
        case 7: mark.addRectangle (x, y + h * 0.35f, w, h * 0.3f); break;    // horizontal bar
        case 8: mark.addRectangle (x + w * 0.35f, y, w * 0.3f, h); break;    // vertical bar
        case 9: mark.addEllipse (x + w * 0.25f, y + h * 0.25f, w * 0.5f, h * 0.5f); break;   // dot
        case 10: mark.addTriangle (x + w * 0.5f, y, x + w, y + h, x, y + h); break;          // triangle, apex up
        case 11: mark.addTriangle (x + w, y + h * 0.5f, x, y + h, x, y); break;              // apex right
        case 12: mark.addTriangle (x + w * 0.5f, y + h, x, y, x + w, y); break;              // apex down
        case 13: mark.addTriangle (x, y + h * 0.5f, x + w, y, x + w, y + h); break;          // apex left
        case 15: mark.addRectangle (x + w * 0.25f, y + h * 0.25f, w * 0.5f, h * 0.5f); break;   // centred square
        case 16:   // slim white X, corner to corner
        {
            const float t = w * 0.16f, len = std::sqrt (w * w + h * h) * 0.92f;
            for (float angle : { 0.7853982f, -0.7853982f })
            {
                juce::Path bar;
                bar.addRectangle (-len * 0.5f, -t * 0.5f, len, t);
                bar.applyTransform (juce::AffineTransform::rotation (angle).translated (x + w * 0.5f, y + h * 0.5f));
                mark.addPath (bar);
            }
            break;
        }
        default:                                                                              // cross, as one outline
        {
            const float a0 = 0.35f, a1 = 0.65f;
            mark.startNewSubPath (x + w * a0, y);
            mark.lineTo (x + w * a1, y);          mark.lineTo (x + w * a1, y + h * a0);
            mark.lineTo (x + w, y + h * a0);      mark.lineTo (x + w, y + h * a1);
            mark.lineTo (x + w * a1, y + h * a1); mark.lineTo (x + w * a1, y + h);
            mark.lineTo (x + w * a0, y + h);      mark.lineTo (x + w * a0, y + h * a1);
            mark.lineTo (x, y + h * a1);          mark.lineTo (x, y + h * a0);
            mark.lineTo (x + w * a0, y + h * a0); mark.closeSubPath();
            break;
        }
    }
    if (mark.isEmpty())
        return;

    juce::Graphics::ScopedSaveState state (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);
    g.setColour (shape == 16 ? juce::Colours::white : grey);
    g.fillPath (mark);
    g.setColour (juce::Colours::black.withAlpha (0.85f));   // thin dark line between mark and colour
    g.strokePath (mark, juce::PathStrokeType (1.0f));
}

}
