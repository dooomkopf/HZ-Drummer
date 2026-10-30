#pragma once

// Grid of the raster editor (implementation detail of PatternEditor, which grants it access).
// One header per bar (meter, grid and bar menu) and one row per lane; click sets/removes a hit,
// shift = accent, alt = ghost, vertical drag changes the velocity, right click cycles it.

#include "app/EditorStyle.h"
#include "app/PatternEditor.h"
#include "engine/MeterCatalog.h"
#include "engine/PatternEdit.h"

// only included by the editor's implementation files
using namespace editorstyle;

// Editor grid: one header per bar plus one row per lane; the width follows from the number of steps.
class EditorGrid : public juce::Component
{
public:
    explicit EditorGrid (PatternEditor& o) : owner (o) {}

    struct Column
    {
        hz::Rational pos;
        int bar = 0;
        float x = 0, w = 0;
        bool barStart = false, beatStart = false, groupStart = false;
    };

    struct BarHeader
    {
        juce::Rectangle<int> area, meterArea, gridArea;
    };

    void layoutFor (int availableWidth)
    {
        const auto& p = owner.pattern;
        int totalSteps = 0;
        for (auto& b : p.bars)
            totalSteps += (int) (b.lengthInQuarters() / (b.grid * hz::Rational (4))).num;

        const float cellW = juce::jmax ((float) minCellW, (float) availableWidth / (float) juce::jmax (1, totalSteps));
        columns.clear();
        headers.clear();

        float x = 0;
        for (int bi = 0; bi < (int) p.bars.size(); ++bi)
        {
            const auto& b = p.bars[(size_t) bi];
            const auto start = p.barStart (bi);
            const auto step = b.grid * hz::Rational (4);
            const int steps = (int) (b.lengthInQuarters() / step).num;
            const auto pulseLen = hz::Rational (4, b.meter.denominator);
            const float barX = x;

            for (int s = 0; s < steps; ++s)
            {
                Column c;
                c.pos = start + hz::Rational (s) * step;
                c.bar = bi;
                c.x = x;
                c.w = cellW;
                c.barStart = s == 0;
                const auto rel = hz::Rational (s) * step / pulseLen;    // in pulses
                c.beatStart = rel.den == 1;
                c.groupStart = c.beatStart && b.meter.accentAt ((int) rel.num) != hz::Meter::Accent::Weak;
                columns.push_back (c);
                x += cellW;
            }

            BarHeader h;
            h.area = juce::Rectangle<float> (barX, 0, x - barX, (float) headerH).toNearestInt();
            auto r = h.area.reduced (4, 4);
            r.removeFromLeft (juce::jmin (54, r.getWidth() / 3));   // "Takt n" label
            h.meterArea = r.removeFromLeft (r.getWidth() / 2).reduced (2, 0);
            h.gridArea = r.reduced (2, 0);
            headers.push_back (h);
        }

        setSize ((int) std::ceil (x) + 1, headerH + owner.rowH * (int) owner.lanes.size());
        repaint();
    }

    // how far right a hit at pos sounds because of the swing (0 if it does not swing)
    float swingOffsetPx (hz::Rational pos) const
    {
        const auto& p = owner.pattern;
        const double shift = hz::swingShift (p, pos);
        if (shift <= 0.0)
            return 0.0f;
        const int bi = p.barAt (pos);
        return (float) (shift / p.bars[(size_t) bi].lengthInQuarters().toDouble()) * (float) headers[(size_t) bi].area.getWidth();
    }

    float xForPos (hz::Rational pos) const
    {
        const auto& p = owner.pattern;
        const int bi = p.barAt (pos);
        const auto& h = headers[(size_t) bi];
        const auto start = p.barStart (bi);
        const double frac = ((pos - start) / p.bars[(size_t) bi].lengthInQuarters()).toDouble();
        return (float) h.area.getX() + (float) frac * (float) h.area.getWidth();
    }

    int columnAt (float x) const
    {
        for (int i = 0; i < (int) columns.size(); ++i)
            if (x >= columns[(size_t) i].x && x < columns[(size_t) i].x + columns[(size_t) i].w)
                return i;
        return -1;
    }

    int laneAt (int y) const
    {
        const int l = (y - headerH) / owner.rowH;
        return y >= headerH && l < (int) owner.lanes.size() ? l : -1;
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.pattern;
        g.fillAll (background);

        // playhead: highlight the current pulse
        const auto ph = owner.host.getPlayhead();
        if (ph.playing && ph.bar >= 0 && ph.bar < (int) p.bars.size())
        {
            const auto& b = p.bars[(size_t) ph.bar];
            const auto from = p.barStart (ph.bar) + hz::Rational ((std::int64_t) ph.pulse * 4, b.meter.denominator);
            const auto to = from + hz::Rational (4, b.meter.denominator);
            const float x0 = xForPos (from);
            const float x1 = to >= p.lengthInQuarters() ? (float) getWidth() - 1 : xForPos (to);
            g.setColour (playhead);
            g.fillRect (juce::Rectangle<float> (x0, (float) headerH, x1 - x0, (float) (getHeight() - headerH)));
        }

        // header per bar
        for (int bi = 0; bi < (int) headers.size(); ++bi)
        {
            const auto& h = headers[(size_t) bi];
            const auto& b = p.bars[(size_t) bi];
            g.setColour (panel);
            g.fillRect (h.area.reduced (1, 2));
            g.setColour (textMain);
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            g.drawText ("Takt " + juce::String (bi + 1), h.area.reduced (6, 0).withWidth (54), juce::Justification::centredLeft);

            g.setFont (juce::FontOptions (14.0f));
            g.setColour (juce::Colour (0xffe89a2c));
            g.drawFittedText (juce::String (hz::meterLabel (b.meter)) + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")),
                              h.meterArea, juce::Justification::centredLeft, 1);
            // the grid; light red if some hits have no place in it (diamonds): its menu then suggests one that has
            const auto gridText = juce::String (b.grid.num) + "/" + juce::String (b.grid.den) + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe"));
            g.setColour (gridHoldsHits (p, bi, b.grid) ? textDim : hintRed);
            g.drawFittedText (gridText, h.gridArea, juce::Justification::centredLeft, 1);
            if (bi == 0 && owner.swingHint)   // behind the grid of bar 1: swing is set, but finds no place
            {
                const int used = (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (14.0f)), gridText)) + 12;
                g.setColour (hintRed);
                g.setFont (juce::FontOptions (13.0f));
                g.drawFittedText ("Swing wirkt in diesem Raster auf keine Stelle", h.gridArea.withTrimmedLeft (used),
                                  juce::Justification::centredLeft, 2, 0.9f);
            }
        }

        // cells; with swing on, the places it moves (the "und" of eighths, "e" and "a" of sixteenths) lightly tinted
        std::vector<bool> swingPlace;
        for (const auto& c : columns)
            swingPlace.push_back (hz::swingShift (p, c.pos) > 0.0);
        for (int li = 0; li < (int) owner.lanes.size(); ++li)
        {
            const int y = headerH + li * owner.rowH;
            for (size_t ci = 0; ci < columns.size(); ++ci)
            {
                const auto& c = columns[ci];
                g.setColour (swingPlace[ci] ? cellSwing : c.beatStart ? cellB : cellA);
                g.fillRect (juce::Rectangle<float> (c.x + 1, (float) y + 1, c.w - 2, (float) owner.rowH - 2));
            }
        }

        // bar lines, pulses, groups
        for (const auto& c : columns)
        {
            if (! (c.barStart || c.groupStart || c.beatStart))
                continue;
            g.setColour (c.barStart ? lineBar : c.groupStart ? lineBeat.brighter (0.4f) : lineBeat);
            g.fillRect (juce::Rectangle<float> (c.x - (c.barStart ? 1.0f : 0.5f), (float) headerH, c.barStart ? 2.0f : 1.0f,
                                                (float) (getHeight() - headerH)));
        }

        // separator between hands and feet
        for (int li = 1; li < (int) owner.lanes.size(); ++li)
            if (hz::roleFor (owner.lanes[(size_t) li].voice) != hz::roleFor (owner.lanes[(size_t) li - 1].voice))
            {
                g.setColour (lineBar.withAlpha (0.6f));
                g.fillRect (0, headerH + li * owner.rowH - 1, getWidth(), 2);
            }

        // a hit played later/earlier: "+20ms" in one line; in a narrow place it gets narrower rather than cut off
        auto drawLag = [&g] (int ms, juce::Rectangle<float> area, juce::Justification where)
        {
            g.setFont (juce::FontOptions (13.0f));
            g.drawFittedText ((ms > 0 ? juce::String ("+") : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"))) + juce::String (std::abs (ms)) + "ms",
                              area.toNearestInt(), where, 1, 0.5f);
        };

        // hits
        for (const auto& l : p.layers)
            for (const auto& e : l.events)
            {
                const int li = owner.laneIndex (e.voice);
                if (li < 0)
                    continue;
                const float y = (float) (headerH + li * owner.rowH);
                const auto col = theme::voiceColour (e.voice);
                const int ci = columnFor (e.pos);

                if (ci >= 0)
                {
                    const auto& c = columns[(size_t) ci];
                    // size and brightness show the velocity
                    const float size = 0.35f + 0.65f * e.velocity;
                    auto r = juce::Rectangle<float> (c.x + 2, y + 2, c.w - 4, (float) owner.rowH - 4);
                    r = r.withSizeKeepingCentre (r.getWidth() * size, r.getHeight() * size);
                    // swing: drawn where it sounds; its grid place (where it is clicked) stays as a faint outline
                    const float swingDx = swingOffsetPx (e.pos);
                    if (swingDx > 0.0f)
                    {
                        g.setColour (col.withAlpha (0.35f));
                        g.drawRoundedRectangle (r, 3.0f, 1.0f);
                        r = r.translated (swingDx, 0.0f);
                    }
                    g.setColour (col.withAlpha (0.45f + 0.55f * e.velocity));
                    g.fillRoundedRectangle (r, 3.0f);
                    if (e.velocity >= 0.95f)
                    {
                        g.setColour (juce::Colours::white);
                        g.drawRoundedRectangle (r, 3.0f, 1.5f);
                    }
                    if (const int ms = juce::roundToInt (e.lagMs); ms != 0)   // played later/earlier: the ms on the hit
                    {
                        // dark on light hits (snare), white on the others
                        g.setColour (col.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff1c1c20) : juce::Colours::white);
                        drawLag (ms, juce::Rectangle<float> (c.x + swingDx, y, c.w, (float) owner.rowH), juce::Justification::centred);
                    }
                }
                else
                {
                    // not on the grid (e.g. polyrhythm): diamond at the exact position. Swing as on the grid:
                    // drawn where it sounds, its place (where it is clicked) stays as a faint outline
                    const float x = xForPos (e.pos), swingDx = swingOffsetPx (e.pos);
                    const float s = 5.0f + 4.0f * e.velocity, cy = y + owner.rowH * 0.5f;
                    auto diamond = [s, cy] (float centre)
                    {
                        juce::Path d;
                        d.startNewSubPath (centre - s, cy);
                        d.lineTo (centre, cy - s);
                        d.lineTo (centre + s, cy);
                        d.lineTo (centre, cy + s);
                        d.closeSubPath();
                        return d;
                    };
                    if (swingDx > 0.0f)
                    {
                        g.setColour (col.withAlpha (0.35f));
                        g.strokePath (diamond (x), juce::PathStrokeType (1.0f));
                    }
                    g.setColour (col.withAlpha (0.5f + 0.5f * e.velocity));
                    g.fillPath (diamond (x + swingDx));
                    if (const int ms = juce::roundToInt (e.lagMs); ms != 0)
                    {
                        g.setColour (juce::Colours::white);   // beside the small diamond, on the dark cells
                        drawLag (ms, juce::Rectangle<float> (x + swingDx + s + 2.0f, y, 44.0f, (float) owner.rowH), juce::Justification::centredLeft);
                    }
                }
            }
    }

    int columnFor (hz::Rational pos) const
    {
        for (int i = 0; i < (int) columns.size(); ++i)
            if (columns[(size_t) i].pos == pos)
                return i;
        return -1;
    }

    bool isDragging() const { return drag.active; }
    void cancelDrag() { drag.active = false; }

    // A mouse-up can get lost (desktop glitch, another window grabbing the pointer). The drag would
    // then stay open for good and the editor would report unsaved changes forever. Once the button
    // has been up for a few timer ticks, keep what the drag changed; a plain click is not completed.
    void finishLostDrag()
    {
        if (! drag.active)
            return;
        if (juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
        {
            drag.upTicks = 0;
            return;
        }
        if (++drag.upTicks < 3)
            return;   // the mouse-up may still be on its way
        drag.active = false;
        owner.changed (drag.before);
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = {};
        const auto pt = e.getPosition();

        if (pt.y < headerH)
        {
            for (int bi = 0; bi < (int) headers.size(); ++bi)
            {
                const auto& h = headers[(size_t) bi];
                if (! h.area.contains (pt))
                    continue;
                if (e.mods.isPopupMenu())          owner.showBarMenu (bi);
                else if (h.meterArea.contains (pt)) owner.showMeterMenuForBar (bi);
                else if (h.gridArea.contains (pt))  owner.showGridMenuForBar (bi);
                else                                owner.showBarMenu (bi);
                return;
            }
            return;
        }

        const int li = laneAt (pt.y);
        if (li < 0)
            return;
        const auto voice = owner.lanes[(size_t) li].voice;
        const int ci = columnAt ((float) pt.x);

        // right click: cycle velocity (ghost -> normal -> accent), or delete a diamond
        if (e.mods.isPopupMenu())
        {
            const auto before = owner.pattern;
            if (const auto* off = offGridNear (voice, (float) pt.x))
            {
                hz::removeEvent (owner.pattern, voice, off->pos);
            }
            else if (ci >= 0)
            {
                const auto pos = columns[(size_t) ci].pos;
                if (const auto* ev = hz::findEvent (owner.pattern, voice, pos))
                {
                    const float v = ev->velocity < 0.5f ? normalVel : ev->velocity < 0.95f ? accentVel : ghostVel;
                    hz::setEventVelocity (owner.pattern, voice, pos, v);
                }
                else
                {
                    hz::toggleEvent (owner.pattern, voice, pos, ghostVel);
                }
            }
            owner.changed (before);
            return;
        }

        if (ci < 0)
            return;

        // Ctrl+click on a hit: play it a little later ("laid back"), Ctrl+Shift+click earlier; one step per click
        if (e.mods.isCommandDown())
        {
            const auto pos = columns[(size_t) ci].pos;
            if (const auto* ev = hz::findEvent (owner.pattern, voice, pos))
            {
                const auto before = owner.pattern;
                const float step = e.mods.isShiftDown() ? -lagStepMs : lagStepMs;
                hz::setEventLag (owner.pattern, voice, pos, juce::jlimit (-maxLagMs, maxLagMs, ev->lagMs + step));
                owner.changed (before);
            }
            return;
        }

        drag.active = true;
        drag.before = owner.pattern;
        drag.voice = voice;
        drag.pos = columns[(size_t) ci].pos;
        drag.startY = pt.y;

        if (const auto* ev = hz::findEvent (owner.pattern, voice, drag.pos))
        {
            drag.existed = true;
            drag.startVel = ev->velocity;
        }
        else
        {
            drag.startVel = e.mods.isShiftDown() ? accentVel : e.mods.isAltDown() ? ghostVel : normalVel;
            hz::toggleEvent (owner.pattern, voice, drag.pos, drag.startVel);
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! drag.active || std::abs (e.getPosition().y - drag.startY) < 4)
            return;
        drag.moved = true;
        const float v = juce::jlimit (0.05f, 1.0f, drag.startVel + (float) (drag.startY - e.getPosition().y) / 120.0f);
        hz::setEventVelocity (owner.pattern, drag.voice, drag.pos, v);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! drag.active)
            return;
        if (drag.existed && ! drag.moved)
            hz::removeEvent (owner.pattern, drag.voice, drag.pos);   // plain click on a hit: delete it
        drag.active = false;
        owner.changed (drag.before);
    }

private:
    const hz::PatternEvent* offGridNear (hz::Voice v, float x) const
    {
        for (const auto& l : owner.pattern.layers)
            for (const auto& e : l.events)
                if (e.voice == v && columnFor (e.pos) < 0 && std::abs (xForPos (e.pos) - x) < 8.0f)
                    return &e;
        return nullptr;
    }

    struct Drag
    {
        bool active = false, existed = false, moved = false;
        hz::Pattern before;
        hz::Voice voice;
        hz::Rational pos;
        int startY = 0;
        float startVel = normalVel;
        int upTicks = 0;   // timer ticks without a held button, see finishLostDrag()
    };

    PatternEditor& owner;
    std::vector<Column> columns;
    std::vector<BarHeader> headers;
    Drag drag;
};
