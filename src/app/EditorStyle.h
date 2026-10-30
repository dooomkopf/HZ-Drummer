#pragma once

// Colours, sizes and option lists shared by the raster editor's parts (PatternEditor, EditorGrid).

#include "app/PatternIO.h"
#include "app/views/Theme.h"
#include "engine/Pattern.h"

#include <juce_graphics/juce_graphics.h>

namespace editorstyle
{
    const juce::Colour background = theme::background;
    const juce::Colour panel      = theme::panel;
    const juce::Colour textMain   = theme::textMain;
    const juce::Colour textDim    = theme::textDim;
    const juce::Colour cellA      { 0xff202026 };
    const juce::Colour cellB      { 0xff26262d };
    const juce::Colour cellSwing  { 0xff3a3028 };   // a place the chosen swing moves, lightly tinted
    const juce::Colour hintRed    { 0xffe07a70 };   // the light red of all help lines
    const juce::Colour lineBeat   { 0xff4a4a55 };
    const juce::Colour lineBar    { 0xffb0b0bc };
    const juce::Colour playhead   { 0x3348a0ff };

    constexpr int headerH = 44;      // bar header height
    constexpr int laneNameW = 165;   // width of the lane name column ("Ride-Glocke", "HiHat-Pedal" next to M / S)
    constexpr int minCellW = 16;     // narrowest grid cell
    constexpr int maxUndo = 200;

    constexpr float ghostVel = 0.3f, normalVel = 0.75f, accentVel = 1.0f;
    constexpr float lagStepMs = 20.0f;   // Ctrl+click on a hit: this much later per click, Ctrl+Shift+click earlier ...
    constexpr float maxLagMs = 100.0f;   // ... up to this far in either direction

    // Content of a pattern without its identity (id, revision, origin): compared to decide "unsaved?"
    inline juce::String contentKey (hz::Pattern p)
    {
        p.id.clear();
        p.revision = 0;
        p.origin = {};
        return juce::JSON::toString (patternToJson (p), true);
    }

    struct GridOption { const char* label; hz::Rational grid; };
    inline const GridOption gridOptions[] = {
        { "Viertel (1/4)", { 1, 4 } }, { "Achtel (1/8)", { 1, 8 } }, { "Sechzehntel (1/16)", { 1, 16 } },
        { "32tel (1/32)", { 1, 32 } }, { "Achteltriolen (1/12)", { 1, 12 } }, { "16tel-Triolen (1/24)", { 1, 24 } },
        { "32tel-Triolen (1/48)", { 1, 48 } },
    };

    // Does every hit of the bar have a place in this grid? (The editor draws the others as diamonds.)
    inline bool gridHoldsHits (const hz::Pattern& p, int bar, hz::Rational grid)
    {
        const auto start = p.barStart (bar), step = grid * hz::Rational (4);
        for (const auto& l : p.layers)
            for (const auto& e : l.events)
                if (p.barAt (e.pos) == bar && ((e.pos - start) / step).den != 1)
                    return false;
        return true;
    }

    // Has this grid a place in the bar that the pattern's swing moves? (The editor tints those columns.)
    inline bool barHasSwingPlace (const hz::Pattern& p, int bar, hz::Rational grid)
    {
        const auto step = grid * hz::Rational (4);   // the grid places, as EditorGrid lays them out
        const int steps = (int) (p.bars[(size_t) bar].lengthInQuarters() / step).num;
        for (int s = 0; s < steps; ++s)
            if (hz::swingShift (p, p.barStart (bar) + hz::Rational (s) * step) > 0.0)
                return true;
        return false;
    }

    // The grid suggested for a bar with diamonds: the coarsest option that divides the bar and has a place
    // for every hit; forSwing: one that also has places the swing moves. {0, 1} if there is none.
    inline hz::Rational fittingGrid (const hz::Pattern& p, int bar, bool forSwing = false)
    {
        hz::Rational best { 0, 1 };
        for (const auto& o : gridOptions)
            if (o.grid > best && (p.bars[(size_t) bar].lengthInQuarters() / (o.grid * hz::Rational (4))).den == 1
                && gridHoldsHits (p, bar, o.grid) && (! forSwing || barHasSwingPlace (p, bar, o.grid)))
                best = o.grid;
        return best;
    }

    struct SwingOption { const char* label; hz::Rational sub; float ratio; };
    inline const SwingOption swingOptions[] = {
        { "Kein Swing", { 1, 8 }, 0.5f }, { "Achtel leicht (58 %)", { 1, 8 }, 0.58f }, { "Achtel triolisch (67 %)", { 1, 8 }, 0.667f },
        { "Achtel stark (72 %)", { 1, 8 }, 0.72f }, { "16tel leicht (58 %)", { 1, 16 }, 0.58f }, { "16tel triolisch (67 %)", { 1, 16 }, 0.667f },
    };

    // Swing moves only the offbeats of its subdivision: is there one here, a grid place or a hit? (Not in the
    // grid 1/12, which has no straight "und", nor in a pattern of quarters and triplets.) False without swing.
    inline bool swingFindsPlace (const hz::Pattern& p)
    {
        for (int bi = 0; bi < (int) p.bars.size(); ++bi)
            if (barHasSwingPlace (p, bi, p.bars[(size_t) bi].grid))
                return true;
        for (const auto& l : p.layers)
            for (const auto& e : l.events)
                if (hz::swingShift (p, e.pos) > 0.0)
                    return true;
        return false;
    }
}
