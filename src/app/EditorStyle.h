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
    const juce::Colour lineBeat   { 0xff4a4a55 };
    const juce::Colour lineBar    { 0xffb0b0bc };
    const juce::Colour playhead   { 0x3348a0ff };

    constexpr int headerH = 44;      // bar header height
    constexpr int laneNameW = 130;   // width of the lane name column
    constexpr int minCellW = 16;     // narrowest grid cell
    constexpr int maxUndo = 200;

    constexpr float ghostVel = 0.3f, normalVel = 0.75f, accentVel = 1.0f;

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
    };

    struct SwingOption { const char* label; hz::Rational sub; float ratio; };
    inline const SwingOption swingOptions[] = {
        { "Kein Swing", { 1, 8 }, 0.5f }, { "Achtel leicht", { 1, 8 }, 0.58f }, { "Achtel triolisch", { 1, 8 }, 0.667f },
        { "Achtel stark", { 1, 8 }, 0.72f }, { "16tel leicht", { 1, 16 }, 0.58f }, { "16tel triolisch", { 1, 16 }, 0.667f },
    };
}
