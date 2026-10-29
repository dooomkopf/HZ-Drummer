#include "app/views/BeatDisplay.h"

#include "app/views/Theme.h"

namespace
{
    constexpr double litMs = 180.0;   // how long a hit stays lit

    int laneOf (hz::Voice v)
    {
        using I = hz::Instrument;
        if (v.instrument == I::Kick || (v.instrument == I::HiHat && v.articulation == hz::Articulation::Pedal))
            return 2;
        if (v.instrument == I::Snare || v.instrument == I::Tom1 || v.instrument == I::Tom2 || v.instrument == I::Tom3 || v.instrument == I::Tom4)
            return 1;
        return 0;
    }
}

void BeatDisplay::paint (juce::Graphics& g)
{
    if (view == nullptr)
        return;

    auto area = getLocalBounds();
    juce::Rectangle<int> hitArea;
    if (showHits)
        hitArea = area.removeFromBottom (juce::jlimit (80, 160, area.getHeight() / 2));

    std::vector<float> centres;
    float cell = 0.0f;
    paintCircles (g, area.reduced (6, showHits ? 8 : 20), centres, cell);

    if (showHits && view->plan != nullptr && ! view->plan->bars.empty())
        paintHits (g, hitArea, centres, cell);
}

void BeatDisplay::paintCircles (juce::Graphics& g, juce::Rectangle<int> lights, std::vector<float>& centres, float& cell)
{
    const auto& meter = view->meter;
    const int pulse = view->hasPosition() ? view->pulse : -1;
    const int gaps = juce::jmax (0, meter.numGroups - 1);
    const float gapW = 22.0f;
    cell = juce::jmin ((lights.getWidth() - gaps * gapW) / (float) meter.numerator, (float) lights.getHeight());
    const float totalW = cell * (float) meter.numerator + gapW * (float) gaps;
    float x = (float) lights.getCentreX() - totalW / 2.0f;
    const float y = (float) lights.getCentreY() - cell / 2.0f;

    int p = 0;
    for (int gi = 0; gi < meter.numGroups; ++gi)
    {
        for (int k = 0; k < meter.groups[(size_t) gi]; ++k, ++p)
        {
            const auto col = theme::accentColour (meter.accentAt (p));
            auto r = juce::Rectangle<float> (x, y, cell, cell).reduced (cell * 0.12f);
            centres.push_back (r.getCentreX());
            g.setColour (p == pulse ? col : theme::unlit);
            g.fillEllipse (r);
            g.setColour (col.withAlpha (p == pulse ? 1.0f : 0.5f));
            g.drawEllipse (r, 2.0f);

            // single beats (x/4) are counted through, groups (7/8 = 2+2+3) are counted within the group
            g.setColour (p == pulse ? theme::background : theme::textDim);
            g.setFont (juce::FontOptions (cell * 0.32f, juce::Font::bold));
            const int label = meter.groups[(size_t) gi] == 1 ? p + 1 : k + 1;
            g.drawText (juce::String (label), r, juce::Justification::centred);
            x += cell;
        }
        x += gapW;
    }
}

void BeatDisplay::paintHits (juce::Graphics& g, juce::Rectangle<int> hitArea, const std::vector<float>& centres, float cell)
{
    const auto& info = *view->plan;
    const int barIndex = view->hasPosition() ? juce::jlimit (0, (int) info.bars.size() - 1, view->bar) : 0;
    const auto& bar = info.bars[(size_t) barIndex];
    const double qpp = bar.meter.quartersPerPulse();
    const int n = (int) centres.size();
    if (n == 0 || n != bar.meter.numerator)
        return;

    // Bar position (quarters) -> x. Pulse k sits on its circle centre; the bar ends one cell after the
    // last centre. The background spans exactly that range, so the marker never runs past it.
    auto xFor = [&] (double q)
    {
        const double rel = juce::jlimit (0.0, (double) n, (q - bar.start) / qpp);
        const int k = juce::jmin (n - 1, (int) rel);
        const float next = k + 1 < n ? centres[(size_t) k + 1] : centres[(size_t) k] + cell;
        return centres[(size_t) k] + (float) (rel - k) * (next - centres[(size_t) k]);
    };

    auto area = hitArea.withTrimmedTop (24).reduced (0, 4);   // room for the progress bar above
    const float rowH = (float) area.getHeight() / 3.0f;
    const float left = centres.front(), right = centres.back() + cell;

    for (int r = 0; r < 3; ++r)
    {
        g.setColour (r % 2 == 0 ? theme::panel : theme::panel.brighter (0.04f));
        g.fillRect (juce::Rectangle<float> (left, (float) area.getY() + r * rowH, right - left, rowH));
    }

    // pulse lines and small ticks on the half pulses
    for (int k = 0; k < n; ++k)
    {
        g.setColour (theme::accentColour (bar.meter.accentAt (k)).withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (centres[(size_t) k] - 1.0f, (float) area.getY(), 2.0f, (float) area.getHeight()));
        const float mid = xFor (bar.start + (k + 0.5) * qpp);
        g.setColour (theme::textDim.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> (mid - 0.5f, (float) area.getBottom() - 8.0f, 1.0f, 8.0f));
        g.fillRect (juce::Rectangle<float> (mid - 0.5f, (float) area.getY(), 1.0f, 8.0f));
    }

    const double msPerQuarter = view->msPerQuarter();
    auto isOffGrid = [&] (double pos)
    {
        const double sixteenths = (pos - bar.start) / (qpp / 4.0);
        return std::abs (sixteenths - std::round (sixteenths)) > 1.0e-6;
    };
    // milliseconds since a hit was heard (flam main strokes come a little later)
    double nowQ = -1.0;
    auto sinceMs = [&] (const hz::PlanEvent& e) { return (nowQ - e.pos) * msPerQuarter - e.extraDelayMs; };

    if (view->live)
    {
        nowQ = bar.start + (view->pulse + view->pulsePhase()) * qpp;

        // progress bar through the bar; pulses on every hit (more on hits between the sixteenths)
        float pulseAmount = 0.0f;
        for (const auto& e : info.events)
            if (const double since = sinceMs (e); since >= 0.0 && since < 160.0)
                pulseAmount = juce::jmax (pulseAmount, (float) (1.0 - since / 160.0) * (isOffGrid (e.pos) ? 1.0f : 0.6f));

        const float px = juce::jmin (xFor (nowQ), right);
        const float barY = (float) area.getY() - 14.0f;
        g.setColour (theme::unlit);
        g.fillRoundedRectangle (juce::Rectangle<float> (left, barY, right - left, 6.0f), 3.0f);
        g.setColour (theme::progress.withAlpha (0.55f + 0.45f * pulseAmount));
        g.fillRoundedRectangle (juce::Rectangle<float> (left, barY - 2.0f * pulseAmount, px - left, 6.0f + 4.0f * pulseAmount), 3.0f);
        g.setColour (theme::textMain.withAlpha (0.35f + 0.5f * pulseAmount));
        g.fillRect (juce::Rectangle<float> (px - 1.0f - pulseAmount, (float) area.getY(), 2.0f + 2.0f * pulseAmount, (float) area.getHeight()));
    }

    for (const auto& e : info.events)
    {
        if (e.pos < bar.start - 1.0e-9 || e.pos >= bar.start + bar.length - 1.0e-9)
            continue;

        const float x = xFor (e.pos) + (float) (e.extraDelayMs / msPerQuarter / qpp) * cell;   // flam main stroke slightly later
        const float cy = (float) area.getY() + (laneOf (e.voice) + 0.5f) * rowH;
        const double since = sinceMs (e);
        const bool lit = view->live && since >= 0.0 && since < litMs;
        const bool past = view->live && since >= litMs;
        const bool offGrid = isOffGrid (e.pos);

        const float size = 4.0f + 5.0f * e.velocity + (lit ? (offGrid ? 7.0f : 4.0f) : 0.0f);
        const auto col = theme::voiceColour (e.voice).withAlpha (lit ? 1.0f : past ? 0.75f : 0.45f);
        if (lit)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (size * 3.0f, size * 3.0f).withCentre ({ x, cy }));
        }
        g.setColour (col);
        if (offGrid)
        {
            juce::Path d;
            d.addTriangle (x - size, cy, x, cy - size, x + size, cy);
            d.addTriangle (x - size, cy, x, cy + size, x + size, cy);
            g.fillPath (d);
        }
        else
        {
            g.fillEllipse (juce::Rectangle<float> (size * 2.0f, size * 2.0f).withCentre ({ x, cy }));
        }
    }
}
