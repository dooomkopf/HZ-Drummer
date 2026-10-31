// PatternEditor, part 2: the lanes (which, their names, M / S, a preview hit on the name) and painting.

#include "app/PatternEditor.h"

#include "app/EditorGrid.h"
#include "app/Text.h"
#include "app/views/InWindowDialog.h"
#include "engine/MeterCatalog.h"
#include "engine/PatternEdit.h"

int PatternEditor::laneIndex (hz::Voice v) const
{
    for (int i = 0; i < (int) lanes.size(); ++i)
        if (lanes[(size_t) i].voice == v)
            return i;
    return -1;
}

bool PatternEditor::isMuted (hz::Voice v) const
{
    return std::find (mutedVoices.begin(), mutedVoices.end(), v) != mutedVoices.end();
}

bool PatternEditor::isSolo (hz::Voice v) const
{
    return std::find (soloVoices.begin(), soloVoices.end(), v) != soloVoices.end();
}

hz::Pattern PatternEditor::audible() const
{
    if (mutedVoices.empty() && soloVoices.empty())
        return pattern;
    // solo wins: only soloed lanes sound; otherwise everything but the muted ones
    hz::Pattern p = pattern;
    for (auto& layer : p.layers)
        layer.events.erase (std::remove_if (layer.events.begin(), layer.events.end(), [this] (const hz::PatternEvent& ev)
                            { return soloVoices.empty() ? isMuted (ev.voice) : ! isSolo (ev.voice); }),
                            layer.events.end());
    return p;
}

juce::Rectangle<int> PatternEditor::laneSwitch (int lane, bool solo) const
{
    const auto row = juce::Rectangle<int> (8, viewport.getY() + headerH + lane * rowH, laneNameW - 12, rowH);
    const int size = juce::jmin (18, rowH - 4);
    return { row.getRight() - (solo ? size : 2 * size + 4), row.getCentreY() - size / 2, size, size };
}

juce::Rectangle<int> PatternEditor::laneSwitchReset() const
{
    // as high as an M / S box, as wide as both, centred in the bar header row above the lanes
    const auto m = laneSwitch (0, false), s = laneSwitch (0, true);
    return { m.getX(), viewport.getY() + (headerH - m.getHeight()) / 2, s.getRight() - m.getX(), m.getHeight() };
}

void PatternEditor::mouseDown (const juce::MouseEvent& e)
{
    if (pattern.bars.empty())
        return;
    if (laneSwitchReset().contains (e.getPosition()))
    {
        mutedVoices.clear();
        soloVoices.clear();
        host.auditionPattern (audible());
        repaint();
        return;
    }
    for (int i = 0; i < (int) lanes.size(); ++i)
        for (bool solo : { false, true })
            if (laneSwitch (i, solo).contains (e.getPosition()))
            {
                auto& list = solo ? soloVoices : mutedVoices;
                const auto v = lanes[(size_t) i].voice;
                if (auto it = std::find (list.begin(), list.end(), v); it != list.end())
                {
                    list.erase (it);
                }
                else
                {
                    list.push_back (v);
                    // M and S of one lane exclude each other: switching one on switches the other off
                    auto& other = solo ? mutedVoices : soloVoices;
                    other.erase (std::remove (other.begin(), other.end(), v), other.end());
                }
                host.auditionPattern (audible());
                repaint();
                return;
            }

    // a click on a lane name: one hit of that instrument, to hear it before writing anything;
    // a right click: its hits onto another lane (the cymbal of a groove, e.g. hi-hat -> ride)
    for (int i = 0; i < (int) lanes.size(); ++i)
        if (juce::Rectangle<int> (8, viewport.getY() + headerH + i * rowH, laneNameW - 12, rowH).contains (e.getPosition()))
        {
            if (e.mods.isPopupMenu())
                showLaneMenu (i);
            else
                host.previewVoice (lanes[(size_t) i].voice);
            return;
        }
}

void PatternEditor::showLaneMenu (int lane)
{
    const auto from = lanes[(size_t) lane].voice;
    bool used = false;
    for (const auto& layer : pattern.layers)
        for (const auto& e : layer.events)
            used = used || e.voice == from;

    // every lane the kit plays; one undo step
    juce::PopupMenu targets;
    const auto* kit = host.currentKit();
    for (const auto& l : allLanes)
    {
        if (l.voice == from || (kit != nullptr && kit->instrumentFor (l.voice, false) < 0))
            continue;
        targets.addItem (laneLabel (l), [safe = juce::Component::SafePointer<PatternEditor> (this), from, to = l.voice]
        {
            if (safe == nullptr || safe->pattern.bars.empty() || safe->grid->isDragging())
                return;
            const auto before = safe->pattern;
            if (hz::moveLane (safe->pattern, from, to) > 0)
                safe->changed (before);
        });
    }
    juce::PopupMenu m;
    m.addSubMenu ("Übertragen auf"_u, targets, used);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

juce::String PatternEditor::laneLabel (const Lane& l) const
{
    // the kit's real instrument where the voice name would be wrong (no cowbell, no splash in the Aasimonster)
    static const std::pair<const char*, const char*> realNames[] = {
        { "zilbel", "Zil-Bel" }, { "china_8_inch", "China 8\"" }, { "china_10_inch", "China 10\"" }, { "china_18_inch", "China 18\"" },
    };
    if (const auto* kit = host.currentKit())
        if (const int inst = kit->instrumentFor (l.voice, false); inst >= 0)
            for (const auto& [instrument, label] : realNames)
                if (kit->instruments[(size_t) inst].name == instrument)
                    return label;
    return l.name;
}

bool PatternEditor::laneSilent (const Lane& l) const
{
    const auto* kit = host.currentKit();
    return kit != nullptr && kit->instrumentFor (l.voice, false) < 0;
}

void PatternEditor::updateLanes()
{
    lanes.clear();
    const auto* kit = host.currentKit();
    std::vector<int> soundsShown;   // kit instruments that already have a lane
    for (const auto& l : allLanes)
    {
        bool used = false;
        for (const auto& layer : pattern.layers)
            for (const auto& e : layer.events)
                used = used || e.voice == l.voice;
        // with a kit: a lane without a sound of its own (none, or the same as a lane above, e.g. "HiHat halb")
        // only when the pattern uses it, so that nothing becomes invisible
        const int inst = kit != nullptr ? kit->instrumentFor (l.voice, false) : -1;
        if (kit != nullptr && ! used && (inst < 0 || std::find (soundsShown.begin(), soundsShown.end(), inst) != soundsShown.end()))
            continue;
        if (l.core || used || allLanesToggle.getToggleState())
        {
            lanes.push_back (l);
            soundsShown.push_back (inst);
        }
    }
    resized();
    repaint();
}

void PatternEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    if (pattern.bars.empty())   // nothing loaded yet: say how to get a pattern in here
    {
        g.setColour (textDim);
        g.setFont (juce::FontOptions (17.0f));
        g.drawFittedText ("Kein Baustein geladen.\n\nIm Hauptfenster einen Baustein wählen und E drücken (oder \"Bearbeiten\"),\n"
                          "oder im Arranger einen Block wählen und \"Editor\" klicken (oder Rechtsklick, \"Bearbeiten\").\n"
                          "Dann erscheint er hier im Editor."_u,
                          viewport.getBounds().reduced (20), juce::Justification::centred, 5);
        return;
    }

    // lane names on the left, aligned with the grid rows, with the M / S switches at their end
    if (! lanes.empty())
    {
        // above them: all M / S off (dim while none is on)
        const bool any = ! mutedVoices.empty() || ! soloVoices.empty();
        const auto box = laneSwitchReset();
        g.setColour (juce::Colour (0xff3a3a44).withAlpha (any ? 1.0f : 0.5f));
        g.fillRoundedRectangle (box.toFloat(), 3.0f);
        g.setColour (any ? textMain : textDim.withAlpha (0.6f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText ("Reset", box, juce::Justification::centred);
    }
    const int top = viewport.getY() + headerH;
    for (int i = 0; i < (int) lanes.size(); ++i)
    {
        const auto voice = lanes[(size_t) i].voice;
        const bool muted = isMuted (voice), solo = isSolo (voice);
        const bool silent = soloVoices.empty() ? muted : ! solo;
        auto r = juce::Rectangle<int> (8, top + i * rowH, laneNameW - 12, rowH);
        g.setColour (theme::voiceColour (voice).withAlpha (silent ? 0.35f : 1.0f));
        g.fillRoundedRectangle (r.removeFromLeft (5).reduced (0, 5).toFloat(), 2.0f);
        // no sound in the kit: dim and italic (it only shows because the pattern uses it)
        const bool noSound = laneSilent (lanes[(size_t) i]);
        g.setColour (silent || noSound ? textDim : textMain);
        g.setFont (juce::FontOptions (15.0f, noSound ? juce::Font::italic : juce::Font::plain));
        g.drawText (laneLabel (lanes[(size_t) i]), r.withTrimmedLeft (6).withTrimmedRight (2 * laneSwitch (i, false).getWidth() + 10),
                    juce::Justification::centredLeft);
        for (bool s : { false, true })
        {
            const auto box = laneSwitch (i, s);
            const bool on = s ? solo : muted;
            g.setColour (on ? (s ? juce::Colour (0xff2e7a4a) : juce::Colour (0xffb0552a)) : juce::Colour (0xff3a3a44));
            g.fillRoundedRectangle (box.toFloat(), 3.0f);
            g.setColour (on ? textMain : textDim);
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (s ? "S" : "M", box, juce::Justification::centred);
        }
    }

    if (blockMode)   // unmistakable: this is the song's own copy, not the library pattern
    {
        auto banner = getLocalBounds().reduced (12, 6).withTrimmedBottom (46).removeFromBottom (36);
        g.setColour (juce::Colour (0xff2a3f5a));
        g.fillRoundedRectangle (banner.toFloat(), 6.0f);
        g.setColour (textMain);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawFittedText (cardMode ? "Kopie der Karte \"" + blockTitle + "\" aus der Ablage. \"In Ablage übernehmen\" schreibt die Änderung in die Karte, "
                                     "\"Speichern unter\" macht daraus einen neuen Baustein und legt ihn als neue Karte daneben."_u
                                   : "Lokale Kopie: Block \"" + blockTitle + "\" aus dem Ablauf. Änderungen gelten nur in diesem Arrangement. "
                                     "\"In Ablauf übernehmen\" schreibt sie in den Block, \"Speichern unter\" macht daraus einen neuen Baustein."_u,
                          banner.reduced (12, 0), juce::Justification::centredLeft, 2);
    }

}

juce::String PatternEditor::defaultHelp() const
{
    return "Klick: Schlag setzen/löschen  ·  Shift+Klick: Akzent  ·  Alt+Klick: Ghost  ·  Strg(+Shift)+Klick: später (früher), je 10 ms bis 100  ·  hoch/runter ziehen: Lautstärke\n"
           "Rechtsklick: Ghost/Normal/Akzent, nachträglich Strg+Rechtsklick: Flam  ·  M/S an der Spur: stumm/solo (Reset darüber)  ·  Klick auf Spurname: Probeklang, Rechtsklick: übertragen  ·  Kopfzeile: Taktart, Raster, Takt-Menü  ·  Strg+Mausrad: Breite  ·  Strg+Z/Y  ·  Strg+S "_u
           + (cardMode ? juce::String ("in Ablage übernehmen"_u) : blockMode ? juce::String ("in Ablauf übernehmen"_u) : juce::String ("speichern"));
}
