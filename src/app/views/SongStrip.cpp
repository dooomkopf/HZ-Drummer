#include "app/views/SongStrip.h"

#include "app/views/Theme.h"

#include <utility>

int SongStrip::preferredHeight (int width) const
{
    if (view == nullptr || view->spans == nullptr || view->spans->empty())
        return 0;
    SongLayout probe;
    probe.layout (*view->spans, (float) (width - 12));
    return probe.rows * rowH + (probe.rows - 1) * rowGap + 2 * margin;
}

void SongStrip::relayout()
{
    // cheap (a few thousand steps), so simply redone on every paint: no stale layout after a song change
    layout.layout (*view->spans, (float) stripArea().getWidth());
}

juce::Rectangle<float> SongStrip::rowBounds (int row) const
{
    const auto r = stripArea().toFloat();
    const float h = (r.getHeight() - (float) (layout.rows - 1) * rowGap) / (float) layout.rows;
    return { r.getX(), r.getY() + (float) row * (h + rowGap), r.getWidth(), h };
}

void SongStrip::paint (juce::Graphics& g)
{
    if (view == nullptr || view->spans == nullptr || view->spans->empty())
        return;
    relayout();
    if (layout.blocks.empty())
        return;

    const auto& spans = *view->spans;
    const double position = dragBar >= 0 ? (double) dragBar : view->position();

    for (const auto& b : layout.blocks)
    {
        const auto row = rowBounds (b.row);
        auto block = juce::Rectangle<float> (row.getX() + b.x, row.getY(), b.width, row.getHeight()).reduced (1.0f, 0);
        const bool active = b.contains (position);
        const auto col = theme::partColour (b.colour);

        constexpr float footerH = 17.0f;
        const auto footer = block.withTop (block.getBottom() - footerH);
        g.setColour (col.withAlpha (0.85f));
        g.fillRoundedRectangle (block, 5.0f);
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (block, 5.0f);
            g.reduceClipRegion (clip);
            g.setColour (juce::Colour (0xff24242b).withAlpha (0.9f));
            g.fillRect (footer);
        }

        if (b.width / (float) b.bars >= 3.0f)   // thin bar lines, only if there is room
        {
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            for (int k = 1; k < b.bars; ++k)
                g.fillRect (juce::Rectangle<float> (row.getX() + b.xOf (b.firstBar + k) - 0.5f, block.getY() + block.getHeight() * 0.55f, 1.0f,
                                                    block.getHeight() * 0.45f - 2.0f));
        }

        for (size_t k = b.firstSpan + 1; k < b.endSpan; ++k)   // repeat boundaries
        {
            g.setColour (theme::background.withAlpha (0.75f));
            g.fillRect (juce::Rectangle<float> (row.getX() + b.xOf (spans[k].firstBar) - 1.0f, block.getY() + 2.0f, 2.0f, block.getHeight() - 4.0f));
        }

        // same look as the arranger's blocks: light frame, badge at the left
        g.setColour (juce::Colour (0xffc4c4cc).withAlpha (0.75f));
        g.drawRoundedRectangle (block.reduced (0.5f), 5.0f, 1.0f);
        if (active)
        {
            g.setColour (juce::Colour (0xff3cc6b4));
            g.drawRoundedRectangle (block.reduced (1.5f), 5.0f, 3.0f);
        }
        if (b.width >= 44.0f)
        {
            theme::drawSourceBadge (g, juce::Rectangle<float> (block.getX() + 6.0f, footer.getCentreY() - 6.0f, 12.0f, 12.0f), b.colour, b.shape);
            g.setColour (theme::textMain.withAlpha (0.9f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawFittedText (juce::String (b.bars) + (b.bars == 1 ? " Takt" : " Takte"),
                              footer.toNearestInt().withTrimmedLeft (23).withTrimmedRight (5),
                              juce::Justification::centredLeft, 1);
        }

        const auto& first = spans[b.firstSpan];
        juce::String text = first.label;
        if (first.repeats > 1)
        {
            int rep = 0;
            for (size_t k = b.firstSpan; k < b.endSpan; ++k)
                if (position >= spans[k].firstBar && position < spans[k].firstBar + spans[k].numBars)
                    rep = spans[k].repeat + 1;
            text << (rep > 0 ? "  " + juce::String (rep) + "/" + juce::String (first.repeats)
                             : juce::String (juce::CharPointer_UTF8 (" \xc3\x97")) + juce::String (first.repeats));
        }
        g.setColour (theme::textMain);
        g.setFont (juce::FontOptions (b.width < 60.0f ? 12.0f : 15.0f, juce::Font::bold));
        g.drawFittedText (text, block.toNearestInt().reduced (b.width < 60.0f ? 2 : 6, 2).withTrimmedBottom ((int) footerH),
                          juce::Justification::centredLeft, 1);
    }

    if (rangeFrom >= 0)   // a range being picked (Shift + drag)
        paintRange (g, juce::jmin (rangeFrom, rangeTo), juce::jmax (rangeFrom, rangeTo));
    else if (loopShown && loopFirst >= 0)
        paintRange (g, loopFirst, loopLast);

    if (const auto* b = layout.blockAt (position); b != nullptr)   // position marker with glow
    {
        const auto row = rowBounds (b->row);
        const float x = row.getX() + b->xOf (position);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 5.0f, row.getY() - 3.0f, 10.0f, row.getHeight() + 6.0f), 4.0f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 1.5f, row.getY() - 3.0f, 3.0f, row.getHeight() + 6.0f), 1.5f);
    }
}

void SongStrip::setLoopRange (int firstBar, int lastBar, bool shown)
{
    if (firstBar == loopFirst && lastBar == loopLast && shown == loopShown)
        return;
    loopFirst = firstBar;
    loopLast = lastBar;
    loopShown = shown;
    repaint();
}

void SongStrip::paintRange (juce::Graphics& g, int first, int last)
{
    // over every block it touches: a light veil and strong edges where the range begins and ends
    const juce::Colour loopColour (0xffe8c547);   // yellowish: the loop
    for (const auto& b : layout.blocks)
    {
        const int from = juce::jmax (first, b.firstBar), to = juce::jmin (last + 1, b.firstBar + b.bars);
        if (from >= to)
            continue;
        const auto row = rowBounds (b.row);
        const float x0 = row.getX() + b.xOf (from), x1 = row.getX() + b.xOf (to);
        const auto area = juce::Rectangle<float> (x0, row.getY() - 2.0f, x1 - x0, row.getHeight() + 4.0f);
        g.setColour (loopColour.withAlpha (0.22f));
        g.fillRect (area);
        g.setColour (loopColour);
        if (from == first)
            g.fillRect (area.withWidth (3.0f));
        if (to == last + 1)
            g.fillRect (area.withLeft (area.getRight() - 3.0f));
        g.fillRect (area.withHeight (2.0f));
        g.fillRect (area.withTop (area.getBottom() - 2.0f));
    }
}

int SongStrip::barAt (juce::Point<int> p) const
{
    if (layout.blocks.empty())
        return -1;
    const auto r = stripArea().toFloat();
    const float rowPitch = (r.getHeight() - (float) (layout.rows - 1) * rowGap) / (float) layout.rows + rowGap;
    const int row = juce::jlimit (0, layout.rows - 1, (int) (((float) p.y - r.getY()) / rowPitch));
    const auto* b = layout.blockAt ((float) p.x - r.getX(), row);
    if (b == nullptr)
        return -1;
    return juce::jlimit (b->firstBar, b->firstBar + b->bars - 1, (int) std::floor (b->barAt ((float) p.x - r.getX())));
}

bool SongStrip::nearEdge (juce::Point<float> p, int bar, bool atItsEnd) const
{
    constexpr float grip = 14.0f;   // pixels either side of the edge
    for (const auto& b : layout.blocks)
        if (b.contains (bar))
        {
            const auto row = rowBounds (b.row);
            const float x = row.getX() + b.xOf (atItsEnd ? bar + 1 : bar);
            return std::abs (p.x - x) <= grip && p.y >= row.getY() - 6.0f && p.y <= row.getBottom() + 6.0f;
        }
    return false;
}

void SongStrip::mouseMove (const juce::MouseEvent& e)
{
    // with Shift held, an edge of the shown range can be taken hold of: say so with the cursor
    const bool grip = e.mods.isShiftDown() && loopShown && loopFirst >= 0
                   && (nearEdge (e.position, loopFirst, false) || nearEdge (e.position, loopLast, true));
    setMouseCursor (grip ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::PointingHandCursor);
}

void SongStrip::mouseDown (const juce::MouseEvent& e)
{
    ignoreGesture = false;
    if (e.mods.isShiftDown())   // a loop range of whole bars
    {
        if (loopShown && loopFirst >= 0 && nearEdge (e.position, loopFirst, false))
        {
            rangeFrom = loopLast;    // its start moves, its end stays
            rangeTo = loopFirst;
        }
        else if (loopShown && loopFirst >= 0 && nearEdge (e.position, loopLast, true))
        {
            rangeFrom = loopFirst;   // its end moves, its start stays
            rangeTo = loopLast;
        }
        else if (const int bar = barAt (e.getPosition()); loopShown && loopFirst >= 0 && bar >= loopFirst && bar <= loopLast)
        {
            ignoreGesture = true;   // inside the range, away from its edges: nothing (its edges are moved, not redrawn)
            return;
        }
        else
        {
            rangeFrom = rangeTo = bar;   // outside: a new range
        }
        repaint();
        return;
    }
    dragBar = barAt (e.getPosition());
    repaint();
}

void SongStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (ignoreGesture)
        return;
    if (rangeFrom >= 0)
    {
        if (const int bar = barAt (e.getPosition()); bar >= 0)
            rangeTo = bar;
        repaint();
        return;
    }
    dragBar = barAt (e.getPosition());
    repaint();
}

void SongStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int bar = barAt (e.getPosition());
    if (loopShown && loopFirst >= 0 && bar >= loopFirst && bar <= loopLast && onLoopClear)
        onLoopClear();
}

void SongStrip::mouseUp (const juce::MouseEvent&)
{
    if (std::exchange (ignoreGesture, false))
        return;
    if (rangeFrom >= 0)
    {
        const int first = juce::jmin (rangeFrom, rangeTo), last = juce::jmax (rangeFrom, rangeTo);
        rangeFrom = rangeTo = -1;
        if (first >= 0 && onLoopRange)
            onLoopRange (first, last);
        repaint();
        return;
    }
    const int bar = dragBar;
    dragBar = -1;
    if (bar >= 0 && onSeek)
        onSeek (bar);
    repaint();
}
