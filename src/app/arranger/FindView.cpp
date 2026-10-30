#include "app/arranger/FindView.h"

#include "app/Text.h"
#include "app/arranger/ArrangerStyle.h"

using namespace arrangerstyle;

namespace
{
    constexpr int boxX = 10, boxSize = 16;   // checkbox position inside a row
}

FindView::FindView() : list (*this)
{
    addAndMakeVisible (field);
    field.setFont (juce::FontOptions (fontTitle));
    field.setTextToShowWhenEmpty ("Suchen: Name, Stil, Band, 7/8, 180 bpm ..."_u, textDim);
    field.onTextChange = [this] { rebuild(); };
    field.onEscapeKey = [this] { if (onEscape) onEscape(); };
    field.onReturnKey = [this] { if (auto t = targets(); ! t.empty() && onAddToScratch) onAddToScratch (t); };

    addAndMakeVisible (chipLabel);
    chipLabel.setFont (juce::FontOptions (fontSmall));
    chipLabel.setColour (juce::Label::textColourId, accent);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);

    addAndMakeVisible (markedLabel);
    markedLabel.setFont (juce::FontOptions (fontSmall));
    markedLabel.setColour (juce::Label::textColourId, textDim);
    for (auto* b : { &toScratch, &toSequence, &clearMarks })
        addAndMakeVisible (*b);
    toScratch.onClick  = [this] { if (auto t = targets(); ! t.empty() && onAddToScratch)  onAddToScratch (t); };
    toSequence.onClick = [this] { if (auto t = targets(); ! t.empty() && onAddToSequence) onAddToSequence (t); };
    clearMarks.onClick = [this] { marked.clear(); updateActionRow(); list.repaint(); };
    updateActionRow();
}

void FindView::setIndex (const SearchIndex* i)
{
    index = i;
    // marks of entries that no longer exist would be taken over invisibly
    for (auto it = marked.begin(); it != marked.end();)
        it = (index == nullptr || index->indexOf (*it) < 0) ? marked.erase (it) : std::next (it);
    rebuild();
}

void FindView::focusField()
{
    field.grabKeyboardFocus();
    field.selectAll();
}

//==============================================================================
void FindView::rebuild()
{
    rows.clear();
    if (index != nullptr)
    {
        for (auto& g : index->search (field.getText(), perGroup, expanded))
        {
            rows.push_back ({ Row::Header, g.group, -1, g.total });
            for (int i : g.items)
                rows.push_back ({ Row::Item, g.group, i, 0 });
            if ((int) g.items.size() < g.total)
                rows.push_back ({ Row::More, g.group, -1, g.total });
        }
    }

    const auto q = SearchQuery::parse (field.getText());
    juce::StringArray chips;
    if (q.meterChip().isNotEmpty()) chips.add (q.meterChip());
    if (q.tempoChip().isNotEmpty()) chips.add (q.tempoChip());
    chipLabel.setText (chips.joinIntoString ("   "), juce::dontSendNotification);

    if (cursor >= (int) rows.size() || (cursor >= 0 && rows[(size_t) cursor].type != Row::Item))
        cursor = -1;
    if (cursor < 0)
        for (int r = 0; r < (int) rows.size(); ++r)
            if (rows[(size_t) r].type == Row::Item) { cursor = r; break; }

    list.setSize (juce::jmax (10, viewport.getMaximumVisibleWidth()), rows.empty() ? rowH : rowY ((int) rows.size()));
    list.repaint();
    updateActionRow();
}

int FindView::rowHeight (const Row& r) const
{
    return r.type == Row::Item ? rowH : headerH;
}

int FindView::rowY (int row) const
{
    int y = 0;
    for (int r = 0; r < row && r < (int) rows.size(); ++r)
        y += rowHeight (rows[(size_t) r]);
    return y;
}

int FindView::rowAt (int y) const
{
    int top = 0;
    for (int r = 0; r < (int) rows.size(); ++r)
    {
        const int h = rowHeight (rows[(size_t) r]);
        if (y >= top && y < top + h)
            return r;
        top += h;
    }
    return -1;
}

void FindView::moveCursor (int delta, bool extendMarks)
{
    int r = cursor;
    for (int steps = std::abs (delta); steps > 0; --steps)
    {
        int next = r;
        do
            next += delta > 0 ? 1 : -1;
        while (next >= 0 && next < (int) rows.size() && rows[(size_t) next].type != Row::Item);
        if (next < 0 || next >= (int) rows.size())
            break;
        r = next;
    }
    if (r == cursor || r < 0)
        return;
    if (extendMarks && index != nullptr)
    {
        marked.insert (index->item (rows[(size_t) cursor].item).id);
        marked.insert (index->item (rows[(size_t) r].item).id);
    }
    cursor = r;
    scrollToCursor();
    updateActionRow();
    list.repaint();
}

void FindView::scrollToCursor()
{
    if (cursor < 0)
        return;
    const int y = rowY (cursor), h = rowHeight (rows[(size_t) cursor]);
    const auto visible = viewport.getViewArea();
    if (y < visible.getY())
        viewport.setViewPosition (0, y);
    else if (y + h > visible.getBottom())
        viewport.setViewPosition (0, y + h - visible.getHeight());
}

void FindView::toggleMark (int row)
{
    if (index == nullptr || row < 0 || row >= (int) rows.size() || rows[(size_t) row].type != Row::Item)
        return;
    const auto& id = index->item (rows[(size_t) row].item).id;
    if (! marked.erase (id))
        marked.insert (id);
    updateActionRow();
    list.repaint();
}

std::vector<std::string> FindView::targets() const
{
    // Only marks that are visible right now count (in list order). A mark hidden by the current
    // filter must never be taken over unseen: that once put a different block into the sequence.
    std::vector<std::string> ids;
    if (index == nullptr)
        return ids;
    for (auto& r : rows)
        if (r.type == Row::Item && marked.count (index->item (r.item).id))
            ids.push_back (index->item (r.item).id);
    if (ids.empty() && cursor >= 0)
        ids.push_back (index->item (rows[(size_t) cursor].item).id);
    return ids;
}

int FindView::hiddenMarks() const
{
    int visible = 0;
    if (index != nullptr)
        for (auto& r : rows)
            if (r.type == Row::Item && marked.count (index->item (r.item).id))
                ++visible;
    return (int) marked.size() - visible;
}

std::vector<std::string> FindView::dragIds (int row) const
{
    if (index == nullptr || row < 0 || row >= (int) rows.size() || rows[(size_t) row].type != Row::Item)
        return {};
    const auto& id = index->item (rows[(size_t) row].item).id;
    if (marked.count (id) == 0)
        return { id };
    return targets();   // all marked rows, in list order
}

void FindView::updateActionRow()
{
    const int hidden = hiddenMarks();
    const int n = (int) marked.size() - hidden;
    juce::String text = n == 0 ? juce::String ("nichts markiert") : juce::String (n) + " markiert";
    if (hidden > 0)
        text << "  (" << hidden << " ausgeblendet, z" << juce::String (juce::CharPointer_UTF8 ("\xc3\xa4")) << "hlen nicht)";
    markedLabel.setText (text, juce::dontSendNotification);
    const bool any = n > 0 || cursor >= 0;
    toScratch.setEnabled (any);
    toSequence.setEnabled (any);
    clearMarks.setEnabled (n > 0);
}

//==============================================================================
void FindView::resized()
{
    auto area = getLocalBounds();
    field.setBounds (area.removeFromTop (40));
    area.removeFromTop (1);
    chipLabel.setBounds (area.removeFromTop (16));   // the active filters ("7/8", "180 bpm")
    area.removeFromTop (1);

    auto actions = area.removeFromBottom (buttonH);
    clearMarks.setBounds (actions.removeFromRight (96));
    actions.removeFromRight (6);
    toSequence.setBounds (actions.removeFromRight (112));
    actions.removeFromRight (6);
    toScratch.setBounds (actions.removeFromRight (96));
    markedLabel.setBounds (actions);
    area.removeFromBottom (10);

    viewport.setBounds (area);
    list.setSize (juce::jmax (10, viewport.getMaximumVisibleWidth()), juce::jmax (list.getHeight(), 1));
}

void FindView::paint (juce::Graphics& g)
{
    g.setColour (panel);
    g.fillRoundedRectangle (viewport.getBounds().toFloat().expanded (2.0f), 6.0f);
}

bool FindView::keyPressed (const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown()
        && juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) key.getKeyCode()) == 'K')
    {
        focusField();
        return true;
    }
    return false;
}

//==============================================================================
void FindView::List::paint (juce::Graphics& g)
{
    auto& o = owner;
    if (o.index == nullptr)
        return;
    const int w = getWidth();
    int y = 0;
    for (int r = 0; r < (int) o.rows.size(); ++r)
    {
        const auto& row = o.rows[(size_t) r];
        const int h = o.rowHeight (row);
        auto rect = juce::Rectangle<int> (0, y, w, h);
        y += h;
        if (! g.getClipBounds().intersects (rect))
            continue;

        if (row.type == Row::Header)
        {
            g.setColour (textDim);
            g.setFont (juce::FontOptions (fontSmall, juce::Font::bold));
            g.drawText (SearchIndex::groupTitle (row.group).toUpperCase() + "  (" + juce::String (row.total) + ")",
                        rect.reduced (boxX, 0).withTrimmedTop (8), juce::Justification::centredLeft);
            continue;
        }
        if (row.type == Row::More)
        {
            g.setColour (r == hoverRow ? accent.brighter (0.3f) : accent);
            g.setFont (juce::FontOptions (fontSmall));
            g.drawText ("Alle " + juce::String (row.total) + " anzeigen"_u, rect.reduced (boxX + 24, 0), juce::Justification::centredLeft);
            continue;
        }

        const auto& it = o.index->item (row.item);
        const bool isCursor = r == o.cursor, isMarked = o.marked.count (it.id) > 0;
        const bool sounding = o.isAuditioning && o.isAuditioning (it.id);
        if (isCursor && hasKeyboardFocus (false))
            g.setColour (selected);
        else if (isCursor || r == hoverRow)
            g.setColour (cardHover);
        else
            g.setColour (juce::Colours::transparentBlack);
        g.fillRoundedRectangle (rect.toFloat().reduced (2.0f, 1.0f), 5.0f);

        // checkbox
        auto box = juce::Rectangle<float> ((float) boxX, (float) rect.getCentreY() - boxSize / 2.0f, (float) boxSize, (float) boxSize);
        g.setColour (isMarked ? accent : textDim);
        g.drawRoundedRectangle (box, 3.0f, 1.5f);
        if (isMarked)
        {
            g.fillRoundedRectangle (box.reduced (3.5f), 2.0f);
        }

        // playing marker
        if (sounding)
        {
            g.setColour (playing);
            juce::Path tri;
            tri.addTriangle ((float) boxX + 24.0f, (float) rect.getCentreY() - 6.0f, (float) boxX + 24.0f, (float) rect.getCentreY() + 6.0f,
                             (float) boxX + 34.0f, (float) rect.getCentreY());
            g.fillPath (tri);
        }

        auto text = rect.reduced (boxX + 38, 3).withTrimmedRight (6);
        if (it.colour >= 0)   // the colour its blocks will have; a green ring = it is in the sequence
        {
            const auto square = text.removeFromLeft (20).toFloat().withSizeKeepingCentre (16.0f, 16.0f);
            theme::drawSourceBadge (g, square, it.colour, it.shape);
            if (o.isUsed && o.isUsed (it.id))
            {
                g.setColour (inSequence);
                g.drawRoundedRectangle (square.expanded (2.5f), 4.0f, 2.0f);
            }
            text.removeFromLeft (6);
        }
        if (it.favorite)
        {
            g.setColour (juce::Colour (0xffe8c547));
            g.setFont (juce::FontOptions (18.0f));
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85")), text.removeFromRight (22), juce::Justification::centred);
        }
        g.setColour (sounding ? playing : textMain);
        g.setFont (juce::FontOptions (fontText, juce::Font::bold));
        g.drawFittedText (it.title, text.removeFromTop (22), juce::Justification::centredLeft, 1);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (fontSmall - 1.0f));
        const auto sep = juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
        g.drawFittedText ((it.origin.isNotEmpty() ? it.origin + sep : juce::String()) + it.detail, text, juce::Justification::centredLeft, 1);
    }
}

void FindView::List::mouseMove (const juce::MouseEvent& e)
{
    const int r = owner.rowAt (e.y);
    if (r != hoverRow)
    {
        hoverRow = r;
        repaint();
    }
}

void FindView::List::mouseExit (const juce::MouseEvent&)
{
    hoverRow = -1;
    repaint();
}

void FindView::List::mouseDown (const juce::MouseEvent& e)
{
    auto& o = owner;
    dragging = false;
    pressedRow = -1;
    grabKeyboardFocus();
    const int r = o.rowAt (e.y);
    if (r < 0)
        return;
    const auto& row = o.rows[(size_t) r];
    if (row.type == Row::More)
    {
        o.expanded.insert (row.group);
        o.rebuild();
        return;
    }
    if (row.type != Row::Item)
        return;

    o.cursor = r;
    pressedRow = r;
    pressedOnBox = e.x < boxX + boxSize + 8;
    if (pressedOnBox)
        o.toggleMark (r);   // the box: mark only; the row itself auditions on mouseUp (a drag must not)
    o.updateActionRow();
    repaint();
}

void FindView::List::mouseDrag (const juce::MouseEvent& e)
{
    auto& o = owner;
    if (dragging || pressedOnBox || pressedRow < 0 || e.getDistanceFromDragStart() < 6 || o.index == nullptr)
        return;
    const auto ids = o.dragIds (pressedRow);
    if (ids.empty())
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        juce::StringArray joined;
        for (auto& id : ids)
            joined.add (juce::String (id));
        const auto title = ids.size() == 1 ? o.index->item (o.rows[(size_t) pressedRow].item).title
                                           : juce::String ((int) ids.size()) + " Bausteine";
        container->startDragging (dragFind (joined.joinIntoString (",").toStdString()), this, dragImage (title, selected));
    }
}

void FindView::List::mouseUp (const juce::MouseEvent& e)
{
    auto& o = owner;
    const int r = pressedRow;
    pressedRow = -1;
    if (dragging || pressedOnBox || r < 0 || o.rowAt (e.y) != r || e.mods.isPopupMenu() || o.index == nullptr)
        return;
    if (o.onAudition)
        o.onAudition (o.index->item (o.rows[(size_t) r].item).id);
    repaint();
}

bool FindView::List::keyPressed (const juce::KeyPress& key)
{
    auto& o = owner;
    if (key.getModifiers().isCommandDown())
        return false;   // Ctrl+Z/Y/S/K belong to the panel
    const int c = key.getKeyCode();
    const auto ch = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    if (c == juce::KeyPress::downKey)     { o.moveCursor (1, key.getModifiers().isShiftDown()); return true; }
    if (c == juce::KeyPress::upKey)       { o.moveCursor (-1, key.getModifiers().isShiftDown()); return true; }
    if (c == juce::KeyPress::pageDownKey) { o.moveCursor (8, false); return true; }
    if (c == juce::KeyPress::pageUpKey)   { o.moveCursor (-8, false); return true; }
    if (c == juce::KeyPress::spaceKey && o.cursor >= 0 && o.onAudition)
    {
        o.onAudition (o.index->item (o.rows[(size_t) o.cursor].item).id);
        repaint();
        return true;
    }
    if (ch == 'x')                        { o.toggleMark (o.cursor); return true; }
    if (c == juce::KeyPress::returnKey)
    {
        if (auto t = o.targets(); ! t.empty() && o.onAddToScratch)
            o.onAddToScratch (t);
        return true;
    }
    if (c == juce::KeyPress::escapeKey)   { if (o.onEscape) o.onEscape(); return true; }
    return false;
}
