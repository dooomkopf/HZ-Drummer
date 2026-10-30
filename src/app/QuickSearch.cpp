#include "app/QuickSearch.h"
#include "app/Text.h"
#include "app/views/SourceBadge.h"

#include <algorithm>

namespace
{
    const juce::Colour shade      { 0xb0000000 };
    const juce::Colour panel      { 0xff1f1f25 };
    const juce::Colour selectedBg { 0xff2f4f7a };
    const juce::Colour textMain   { 0xffeeeeee };
    const juce::Colour textDim    { 0xff8a8a92 };
    const juce::Colour gold       { 0xffffc23d };

    juce::String normalise (const juce::String& s)
    {
        return s.toLowerCase().replace ("ä"_u, "a").replace ("ö"_u, "o").replace ("ü"_u, "u").replace ("ß"_u, "ss")
                .replace ("–"_u, "-");
    }

    int wordScore (const juce::String& word, const juce::String& text, bool wholeWord, bool allowInitials)
    {
        for (int at = text.indexOf (word); at >= 0; at = text.indexOf (at + 1, word))
        {
            const bool start = at == 0 || ! juce::CharacterFunctions::isLetterOrDigit (text[at - 1]);
            const int end = at + word.length();
            const bool finish = end == text.length() || ! juce::CharacterFunctions::isLetterOrDigit (text[end]);
            if (! wholeWord || (start && finish))
                return 1000 + (start ? 400 : 0) - juce::jmin (at, 300);
        }
        if (wholeWord || ! allowInitials || word.length() < 2)
            return 0;
        juce::String initials;
        bool previousWasLetter = false;
        for (auto c : text)
        {
            const bool letter = juce::CharacterFunctions::isLetterOrDigit (c);
            if (letter && ! previousWasLetter) initials += juce::String::charToString (c);
            previousWasLetter = letter;
        }
        return initials.contains (word) ? 500 : 0;
    }
}

int fuzzyScore (const juce::String& query, const juce::String& text, bool allowInitials)
{
    const auto normalised = normalise (text);
    auto words = juce::StringArray::fromTokens (normalise (query), " ", "");
    words.removeEmptyStrings();
    const bool finished = query.isNotEmpty() && juce::CharacterFunctions::isWhitespace (query.getLastCharacter());
    int total = 0;
    for (int i = 0; i < words.size(); ++i)
    {
        const int score = wordScore (words[i], normalised, finished && i == words.size() - 1, allowInitials);
        if (score == 0) return 0;
        total += score;
    }
    return total;
}

QuickSearch::QuickSearch()
{
    addAndMakeVisible (field);
    field.setFont (juce::FontOptions (22.0f));
    field.getProperties().set ("pill", true);        // drawn by HZLook as a pill
    field.setIndents (18, field.getTopIndent());     // text clear of the round ends
    field.setTextToShowWhenEmpty ("Suchen: Baustein, Arrangement, Stil, Künstler, Taktart (z.B. 7/8) ..."_u, textDim);
    field.onTextChange = [this] { update(); };
    field.onReturnKey = [this] { choose (selected); };
    field.onEscapeKey = [this] { close(); };
    field.addKeyListener (this);
    setWantsKeyboardFocus (false);
}

void QuickSearch::open (std::vector<Item> items)
{
    all = std::move (items);
    field.clear();
    update();
    setVisible (true);
    toFront (false);
    field.grabKeyboardFocus();
}

void QuickSearch::close()
{
    setVisible (false);
    if (auto* p = getParentComponent())
        p->grabKeyboardFocus();
}

void QuickSearch::update()
{
    query = SearchQuery::parse (field.getText());
    std::vector<std::pair<int, int>> scored;   // (score, index)
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& item = all[(size_t) i];
        const int s = query.score (item.title, item.haystack, item.tempo, item.meters, item.favorite);
        if (s > 0)
            scored.push_back ({ s, i });
    }
    std::stable_sort (scored.begin(), scored.end(), [this] (const auto& a, const auto& b)
    {
        if (a.first != b.first) return a.first > b.first;
        return all[(size_t) a.second].title.compareIgnoreCase (all[(size_t) b.second].title) < 0;
    });

    results.clear();
    for (auto& [s, i] : scored)
        results.push_back (i);
    selected = 0;
    first = 0;
    repaint();
}

void QuickSearch::choose (int index)
{
    if (index < 0 || index >= (int) results.size())
        return;
    auto action = all[(size_t) results[(size_t) index]].action;
    close();
    if (action)
        action();
}

bool QuickSearch::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    using KP = juce::KeyPress;
    const int c = key.getKeyCode();
    const int page = juce::jmax (1, visibleRows() - 1);
    if (c == KP::downKey)     { select (selected + 1); return true; }
    if (c == KP::upKey)       { select (selected - 1); return true; }
    if (c == KP::pageDownKey) { select (selected + page); return true; }
    if (c == KP::pageUpKey)   { select (selected - page); return true; }
    // Home/End belong to the text field; with Ctrl they jump through the list
    if (key.getModifiers().isCommandDown() && c == KP::homeKey) { select (0); return true; }
    if (key.getModifiers().isCommandDown() && c == KP::endKey)  { select ((int) results.size() - 1); return true; }
    return false;
}

void QuickSearch::select (int index)
{
    if (results.empty())
        return;
    selected = juce::jlimit (0, (int) results.size() - 1, index);
    const int rows = visibleRows();
    if (selected < first)
        first = selected;
    else if (selected >= first + rows)
        first = selected - rows + 1;
    first = juce::jlimit (0, juce::jmax (0, (int) results.size() - rows), first);
    repaint();
}

int QuickSearch::visibleRows() const
{
    const int fit = (getHeight() - 50 - 90 - 34 - 20) / rowH;   // keep the panel inside the window
    return juce::jlimit (1, maxRows, fit);
}

juce::Rectangle<int> QuickSearch::panelBounds() const
{
    const int w = juce::jmin (900, getWidth() - 60);
    const int rows = juce::jmax (1, juce::jmin ((int) results.size(), visibleRows()));
    return { (getWidth() - w) / 2, 50, w, 90 + rows * rowH + 34 };
}

void QuickSearch::resized()
{
    field.setBounds (panelBounds().reduced (14).removeFromTop (44));
}

int QuickSearch::rowAt (juce::Point<int> p) const
{
    const auto list = panelBounds().reduced (14).withTrimmedTop (76);
    if (! list.contains (p))
        return -1;
    const int r = (p.y - list.getY()) / rowH;
    if (r >= visibleRows())
        return -1;
    return first + r < (int) results.size() ? first + r : -1;
}

void QuickSearch::mouseMove (const juce::MouseEvent& e)
{
    if (const int r = rowAt (e.getPosition()); r >= 0 && r != selected)
    {
        selected = r;
        repaint();
    }
}

void QuickSearch::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int maxFirst = juce::jmax (0, (int) results.size() - visibleRows());
    const int step = wheel.deltaY < 0 ? 3 : wheel.deltaY > 0 ? -3 : 0;
    first = juce::jlimit (0, maxFirst, first + step);
    if (const int r = rowAt (e.getPosition()); r >= 0)
        selected = r;
    else
        selected = juce::jlimit (first, juce::jmax (first, first + visibleRows() - 1), selected);
    repaint();
}

void QuickSearch::mouseDown (const juce::MouseEvent& e)
{
    const int r = rowAt (e.getPosition());
    if (e.mods.isPopupMenu())
        return;   // the player delete button is the sole deletion entry point
    else if (r >= 0 && e.mods.isLeftButtonDown())
        choose (r);
    else if (r < 0 && ! panelBounds().contains (e.getPosition()))
        close();   // a click outside closes
}

void QuickSearch::paint (juce::Graphics& g)
{
    g.fillAll (shade);
    const auto box = panelBounds();
    resized();   // height depends on the number of results

    g.setColour (panel);
    g.fillRoundedRectangle (box.toFloat(), 10.0f);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (14.0f));
    const auto chips = query.hasFilters()
        ? (query.meterChip() + "  " + query.tempoChip()).trim()
        : juce::String ("Wie im Arranger: Name, Band, Taktart oder z. B. 160-190 bpm");
    g.drawText (chips, box.reduced (14).withTrimmedTop (58).withHeight (18), juce::Justification::centredLeft);

    auto list = box.reduced (14).withTrimmedTop (76);
    if (results.empty())
    {
        g.setColour (textDim);
        g.setFont (juce::FontOptions (18.0f));
        g.drawText ("Nichts gefunden", list.removeFromTop (rowH), juce::Justification::centredLeft);
    }

    const int last = juce::jmin ((int) results.size(), first + visibleRows());
    for (int r = first; r < last; ++r)
    {
        const auto& it = all[(size_t) results[(size_t) r]];
        auto row = list.removeFromTop (rowH);
        if (r == selected)
        {
            g.setColour (selectedBg);
            g.fillRoundedRectangle (row.toFloat(), 6.0f);
        }

        auto inner = row.reduced (10, 0);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (it.kind, inner.removeFromLeft (104), juce::Justification::centredLeft);   // wide enough for "Arrangement"

        if (it.favorite)
        {
            g.setColour (gold);
            g.setFont (juce::FontOptions (18.0f));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85")), inner.removeFromRight (24), juce::Justification::centred);
        }

        if (it.colour >= 0)
        {
            theme::drawSourceBadge (g, inner.removeFromLeft (24).withSizeKeepingCentre (18, 18).toFloat(), it.colour, it.shape);
            inner.removeFromLeft (5);
        }
        g.setColour (textMain);
        g.setFont (juce::FontOptions (18.0f));
        g.drawFittedText (it.title, inner.removeFromTop (27), juce::Justification::centredLeft, 1);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText (it.detail, inner, juce::Justification::centredLeft, 1);
    }

    // thin scroll indicator on the right when not everything fits
    if (const int n = (int) results.size(), rows = visibleRows(); n > rows)
    {
        const auto track = box.reduced (14).withTrimmedTop (76).withHeight (rows * rowH).removeFromRight (4).translated (10, 0).toFloat();
        const float h = juce::jmax (20.0f, track.getHeight() * (float) rows / (float) n);
        const float y = track.getY() + (track.getHeight() - h) * (float) first / (float) (n - rows);
        g.setColour (textDim.withAlpha (0.25f));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (textDim);
        g.fillRoundedRectangle (track.withY (y).withHeight (h), 2.0f);
    }

    g.setColour (textDim);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (juce::String (results.size()) + " Treffer   ·   ↑↓ Bild↑↓ wählen   ·   Enter übernehmen   ·   Esc schließen"_u,
                box.reduced (16, 8).removeFromBottom (20), juce::Justification::centredLeft);
}
