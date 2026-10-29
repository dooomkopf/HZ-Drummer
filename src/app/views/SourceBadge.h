#pragma once

#include <juce_graphics/juce_graphics.h>

// The "badge" that identifies a building block everywhere (Finden, Ablage, sequence, song strip):
// a colour from a fixed palette plus a light grey geometric mark with a thin dark separator.
// Colour and mark are derived from the source pattern's id (see Song.cpp, sourceColourIndex /
// sourceShapeIndex), so they never change when parts are renamed, reordered or edited.
//
// Kept apart from the rest of the theme so palette and marks can be tuned on their own.
// The palette was chosen in OKLab: the smallest distance between any two colours is as large as
// possible while white text and the grey mark stay readable on them; gold and anthracite were
// added by request (tools: scratch scripts, 2026-10-29).
namespace theme
{
    constexpr int numPartColours = 12;
    constexpr int numBadgeShapes = 16;
    constexpr int restColourIndex = 100, clickColourIndex = 101;   // rest/click parts: outside the palette

    // Palette colour by index (wraps); rest and click have their own greys
    juce::Colour partColour (int index);

    // The badge: 'r' filled with the colour, the mark (shape 0..numBadgeShapes-1) in light grey on top
    //   0 plain, 1 top half, 2 bottom half, 3 left half, 4 right half, 5 upper-left triangle,
    //   6 lower-right triangle, 7 horizontal bar, 8 vertical bar, 9 dot,
    //   10..13 triangle with its apex up / right / down / left (base along the opposite edge),
    //   14 cross (horizontal and vertical bar combined), 15 centred square (as wide as the dot)
    void drawSourceBadge (juce::Graphics&, juce::Rectangle<float> r, int colourIndex, int shapeIndex);
}
