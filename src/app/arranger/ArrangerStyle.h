#pragma once

#include "app/Song.h"
#include "app/views/HZLook.h"
#include "app/views/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

// Colours, sizes and small drawing helpers shared by the arranger's views
namespace arrangerstyle
{
    const juce::Colour background = theme::background;
    const juce::Colour panel      = theme::panel;
    const juce::Colour card       { 0xff24242b };
    const juce::Colour cardHover  { 0xff2c2c35 };
    const juce::Colour selected   { 0xff2f4f7a };
    const juce::Colour accent     = theme::progress;
    const juce::Colour textMain   = theme::textMain;
    const juce::Colour textDim    = theme::textDim;
    const juce::Colour playing    { 0xff3cc6b4 };
    const juce::Colour inSequence { 0xff4fd45a };   // Ablage card that the sequence uses
    const juce::Colour danger     { 0xff7a2e2e };   // "Ablage leeren"

    // Sizes: a notch larger than JUCE's defaults, this is read on high-resolution laptops
    constexpr float fontTitle = 19.0f, fontText = 16.0f, fontSmall = 14.0f, fontHint = 14.0f;
    constexpr float fontBlock = 18.0f, fontBlockSmall = 15.0f;   // right side (Ablage, Ablauf): a size larger
    constexpr int buttonH = 36;     // toolbar and action rows
    constexpr int rowH = 48;        // result rows
    constexpr int headerH = 30;     // group headers
    constexpr int cardW = 224, cardH = 108, cardGap = 10;
    constexpr float badgeSize = 18.0f;

    // Buttons, menus and dialogs inside the arranger use these fonts (the default is tiny)
    struct Look : HZLook
    {
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::Font (juce::FontOptions (juce::jmin (fontText, (float) buttonHeight * 0.45f), juce::Font::bold));
        }
        juce::Font getPopupMenuFont() override          { return juce::Font (juce::FontOptions (fontText)); }
        juce::Font getAlertWindowTitleFont() override   { return juce::Font (juce::FontOptions (fontTitle, juce::Font::bold)); }
        juce::Font getAlertWindowMessageFont() override { return juce::Font (juce::FontOptions (fontText)); }
        juce::Font getAlertWindowFont() override        { return juce::Font (juce::FontOptions (fontSmall)); }
    };

    // Drag descriptions: where a dragged block comes from
    inline juce::String dragFind (const std::string& id) { return "find:" + juce::String (id); }
    inline juce::String dragScratch (int index)          { return "scratch:" + juce::String (index); }
    inline juce::String dragSequence (int index)         { return "seq:" + juce::String (index); }

    // Lane of a hit in the mini preview: 0 cymbals, 1 snare/toms, 2 kick
    inline int previewLane (hz::Voice v)
    {
        using I = hz::Instrument;
        if (v.instrument == I::Kick || (v.instrument == I::HiHat && v.articulation == hz::Articulation::Pedal))
            return 2;
        if (v.instrument == I::Snare || v.instrument == I::Tom1 || v.instrument == I::Tom2 || v.instrument == I::Tom3 || v.instrument == I::Tom4)
            return 1;
        return 0;
    }

    // Tiny picture of the hits (three lanes), the whole pattern squeezed into 'area'
    inline void drawMiniPreview (juce::Graphics& g, const hz::Pattern& p, juce::Rectangle<float> area)
    {
        const double length = p.lengthInQuarters().toDouble();
        if (length <= 0.0 || p.bars.empty())
            return;
        const float laneH = area.getHeight() / 3.0f;
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        for (int b = 1; b < (int) p.bars.size(); ++b)
        {
            const float x = area.getX() + (float) (p.barStart (b).toDouble() / length) * area.getWidth();
            g.fillRect (juce::Rectangle<float> (x, area.getY(), 1.0f, area.getHeight()));
        }
        for (auto& layer : p.layers)
            for (auto& e : layer.events)
            {
                const float x = area.getX() + (float) (e.pos.toDouble() / length) * area.getWidth();
                const float y = area.getY() + (previewLane (e.voice) + 0.5f) * laneH;
                const float r = 1.2f + 1.3f * e.velocity;
                g.setColour (theme::voiceColour (e.voice).withAlpha (0.85f));
                g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
            }
    }

    // Small pill with the title, shown under the mouse while dragging
    inline juce::ScaledImage dragImage (const juce::String& title, juce::Colour colour)
    {
        const juce::Font font (juce::FontOptions (14.0f, juce::Font::bold));
        const int w = juce::jlimit (80, 260, 24 + (int) juce::GlyphArrangement::getStringWidth (font, title));
        juce::Image img (juce::Image::ARGB, w, 30, true);
        juce::Graphics g (img);
        g.setColour (colour.withAlpha (0.92f));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) w, 30.0f, 8.0f);
        g.setColour (textMain);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawFittedText (title, 10, 0, w - 20, 30, juce::Justification::centredLeft, 1);
        return juce::ScaledImage (img);
    }

    inline juce::Colour partColour (int index)
    {
        return theme::partColour (index);
    }
}
