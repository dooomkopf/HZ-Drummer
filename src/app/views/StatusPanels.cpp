#include "app/views/StatusPanels.h"

#include "app/Text.h"
#include "app/views/Theme.h"
#include "engine/MeterCatalog.h"

namespace
{
    void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& big, const juce::String& caption)
    {
        g.setColour (theme::panelGlass);
        g.fillRoundedRectangle (r.reduced (6).toFloat(), 10.0f);
        auto inner = r.reduced (16);
        g.setColour (theme::textDim);
        g.setFont (juce::FontOptions (18.0f));
        g.drawText (caption, inner.removeFromTop (24), juce::Justification::centredLeft);
        g.setColour (theme::textMain);
        g.setFont (juce::FontOptions ((float) juce::jmin (inner.getHeight(), 110), juce::Font::bold));
        g.drawFittedText (big, inner, juce::Justification::centred, 1);
    }
}

void StatusPanels::paint (juce::Graphics& g)
{
    if (view == nullptr)
        return;
    const auto& v = *view;
    auto area = getLocalBounds();

    const int colW = area.getWidth() / 3;
    const auto& m = v.meter;
    drawPanel (g, area.removeFromLeft (colW), juce::String (m.numerator) + "/" + juce::String (m.denominator),
               m.numGroups == m.numerator ? juce::String ("Taktart") : "Taktart  " + juce::String (hz::groupingLabel (m)));
    drawPanel (g, area.removeFromLeft (colW), juce::String ((int) std::round (v.tempo)), "BPM (Viertel)");

    // bar: inside a song part "bar/length (repeat n/m)", inside a pattern "bar/length (loop n)", metronome: loop count
    juce::String barText ("-"), caption ("Takt");
    const SongSpan* span = nullptr;
    if (v.hasPosition() && v.spans != nullptr)
        for (auto& sp : *v.spans)
            if (v.bar >= sp.firstBar && v.bar < sp.firstBar + sp.numBars)
                span = &sp;

    if (span != nullptr && span->part >= 0)   // a song part (a pattern's own strip has part -1)
    {
        barText = juce::String (v.bar - span->firstBar + 1) + "/" + juce::String (span->numBars);
        caption = "Takt im Teil"_u + (span->repeats > 1 ? "  (Wdh. " + juce::String (span->repeat + 1) + "/" + juce::String (span->repeats) + ")"
                                                        : juce::String());
    }
    else if (v.hasPosition() && v.numBars > 1)
    {
        barText = juce::String (v.bar + 1) + "/" + juce::String (v.numBars);
        caption = "Takt  (Durchlauf " + juce::String (v.loop + 1) + ")";
    }
    else if (v.hasPosition())
    {
        barText = juce::String (v.loop + 1);
    }
    drawPanel (g, area, barText, caption);
}
