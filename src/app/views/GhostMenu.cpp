#include "app/views/GhostMenu.h"

#include <vector>

namespace
{
    const juce::Colour neon { 0xff39ff14 };
    constexpr double fadeSeconds = 0.25;

    double now() { return juce::Time::getMillisecondCounterHiRes() / 1000.0; }

    // neon text with a fine dark shadow, legible over the watermark
    void glowText (juce::Graphics& g, const juce::String& text, int x, int y, int w, int h, float a,
                   juce::Justification just = juce::Justification::centredLeft)
    {
        g.setColour (juce::Colours::black.withAlpha (0.7f * a));
        g.drawText (text, x + 1, y + 1, w, h, just, true);
        g.setColour (neon.withAlpha (a));
        g.drawText (text, x, y, w, h, just, true);
    }
}

GhostMenu::GhostMenu()
{
    setInterceptsMouseClicks (false, false);
    setWantsKeyboardFocus (false);
}

void GhostMenu::show (const Content& c, juce::Rectangle<int> area)
{
    if (c.path.isEmpty() && c.list.isEmpty())
        return;
    content = c;
    if (content.current >= content.list.size())
        content.current = -1;
    const int fit = juce::jmax (3, (area.getHeight() - 2 * padY - rowH * content.path.size()) / rowH);
    shown = juce::jmin (fit, content.list.size());   // the whole list if it fits
    first = juce::jlimit (0, juce::jmax (0, content.list.size() - shown), content.current - shown / 2);

    setBounds (area);
    shownAt = now();
    setVisible (true);
    toFront (false);
    repaint();
    setGhostAlpha (1.0f);
    startTimerHz (30);
}

void GhostMenu::setGhostAlpha (float a)
{
    if (juce::approximatelyEqual (a, alpha))
        return;
    alpha = a;
    repaint();
    if (onAlpha != nullptr)
        onAlpha (alpha);
}

void GhostMenu::timerCallback()
{
    const double age = now() - shownAt;
    setGhostAlpha (age < holdSeconds ? 1.0f : (float) juce::jmax (0.0, 1.0 - (age - holdSeconds) / fadeSeconds));
    if (alpha <= 0.0f)
    {
        stopTimer();
        setVisible (false);
    }
}

void GhostMenu::paint (juce::Graphics& g)
{
    if (alpha <= 0.0f)
        return;

    const juce::Font pathFont (juce::FontOptions (17.0f, juce::Font::bold)), listFont (juce::FontOptions (16.0f)),
                     currentFont (juce::FontOptions (16.0f, juce::Font::bold)), noteFont (juce::FontOptions (13.0f));
    const auto corner = juce::String::fromUTF8 ("\xe2\x94\x94 ");    // tree corner
    const auto pointer = juce::String::fromUTF8 ("\xe2\x96\xb6");    // the current line
    const auto marker = juce::String::fromUTF8 ("\xe2\x96\xaa ");    // small square before Anfang / Ende
    const auto more = juce::String::fromUTF8 ("\xe2\x80\xa6");       // more lines beyond the window

    // the block, line by line: x, text, font, strength, note behind the text
    struct Line { int x; juce::String text; const juce::Font* font; float strength; juce::String note; bool current; };
    std::vector<Line> lines;
    for (int k = 0; k < content.path.size(); ++k)
    {
        const bool last = k == content.path.size() - 1;
        lines.push_back ({ juce::jmax (0, k - 1) * indentW, (k > 0 ? corner : juce::String()) + content.path[k], &pathFont,
                           last ? 0.95f : 0.55f, last ? content.count : juce::String(), false });
    }
    const int listX = juce::jmax (1, content.path.size()) * indentW;   // below the text of the last path line
    for (int i = first; i < first + shown; ++i)
    {
        const bool isCurrent = i == content.current;
        const bool cut = ! isCurrent && ((i == first && first > 0) || (i == first + shown - 1 && i < content.list.size() - 1));
        const juce::String end = i == content.list.size() - 1 ? "Ende" : i == 0 ? "Anfang" : juce::String();
        lines.push_back ({ listX, cut ? more : content.list[i], isCurrent ? &currentFont : &listFont, isCurrent ? 1.0f : 0.7f,
                           cut ? juce::String() : (end.isNotEmpty() ? marker + end : juce::String()), isCurrent });
    }

    int blockW = 0;
    for (const auto& l : lines)
        blockW = juce::jmax (blockW, l.x + (int) std::ceil (juce::GlyphArrangement::getStringWidth (*l.font, l.text))
                                        + (l.note.isNotEmpty() ? 18 + (int) std::ceil (juce::GlyphArrangement::getStringWidth (noteFont, l.note)) : 0));
    const int blockH = rowH * (int) lines.size();
    const int x0 = padX + 20, y0 = padY;   // left and top: it stays put from key to key

    // a light veil and a thin frame around the block: the tree reads as one piece
    const auto frame = juce::Rectangle<int> (0, 0, juce::jmin (getWidth(), x0 + blockW + padX), blockH + 2 * padY).toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.3f * alpha));
    g.fillRoundedRectangle (frame, 8.0f);
    g.setColour (neon.withAlpha (0.25f * alpha));
    g.drawRoundedRectangle (frame, 8.0f, 1.0f);

    int y = y0;
    for (const auto& l : lines)
    {
        g.setFont (*l.font);
        const int textW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (*l.font, l.text));
        if (l.current)
            glowText (g, pointer, x0 + l.x - 20, y, 18, rowH, alpha);
        glowText (g, l.text, x0 + l.x, y, textW + 4, rowH, l.strength * alpha);
        if (l.note.isNotEmpty())
        {
            g.setFont (noteFont);
            glowText (g, l.note, x0 + l.x + textW + 18, y, 160, rowH, 0.6f * alpha);
        }
        y += rowH;
    }
}
