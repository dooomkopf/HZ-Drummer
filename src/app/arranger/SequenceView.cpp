#include "app/arranger/SequenceView.h"

#include "app/Text.h"
#include "app/arranger/ArrangerStyle.h"

using namespace arrangerstyle;

void SequenceView::setSpans (const std::vector<SongSpan>* s)
{
    spans = s;
    relayout();
    repaint();
}

void SequenceView::setErrorText (const juce::String& text)
{
    errorText = text;
    repaint();
}

juce::Rectangle<int> SequenceView::boundsOfPart (int part) const
{
    for (auto& b : layout.blocks)
        if (b.part == part)
            return blockRect (b).toNearestInt();
    return {};
}

void SequenceView::setSelected (int part)
{
    selectedPart = part;
    repaint();
}

void SequenceView::setPlayPosition (double bar)
{
    if (bar != position)
    {
        position = bar;
        repaint();
    }
}

namespace
{
    constexpr float minBlockW = 64.0f;   // names stay readable; long songs scroll instead
    constexpr int rowLimit = 200;
    constexpr float footerH = 24.0f;     // dark strip at the bottom of a block (badge, length)
}

void SequenceView::relayout()
{
    if (spans != nullptr)
        layout.layout (*spans, (float) (getWidth() - 2 * margin), minBlockW, rowLimit);
    else
        layout = {};
}

int SequenceView::preferredHeight (int width) const
{
    SongLayout probe;
    if (spans != nullptr)
        probe.layout (*spans, (float) (width - 2 * margin), minBlockW, rowLimit);
    return juce::jmax (1, probe.rows) * rowH + (juce::jmax (1, probe.rows) - 1) * rowGap + 2 * margin;
}

juce::Rectangle<float> SequenceView::rowBounds (int row) const
{
    const auto r = getLocalBounds().reduced (margin).toFloat();
    return { r.getX(), r.getY() + (float) row * (rowH + rowGap), r.getWidth(), (float) rowH };
}

juce::Rectangle<float> SequenceView::blockRect (const SongLayout::Block& b) const
{
    const auto row = rowBounds (b.row);
    return { row.getX() + b.x, row.getY(), b.width, row.getHeight() };
}

const SongLayout::Block* SequenceView::blockAt (juce::Point<int> p) const
{
    for (auto& b : layout.blocks)
        if (blockRect (b).contains (p.toFloat()))
            return &b;
    return nullptr;
}

int SequenceView::insertIndexAt (juce::Point<int> p) const
{
    if (spans == nullptr || layout.blocks.empty())
        return 0;
    const int row = juce::jlimit (0, layout.rows - 1, (int) (((float) p.y - (float) margin) / (float) (rowH + rowGap)));
    int after = -1;   // last block in this row left of the mouse centre
    for (auto& b : layout.blocks)
    {
        if (b.row < row)
            after = b.part;
        else if (b.row == row && (float) p.x >= blockRect (b).getCentreX())
            after = b.part;
    }
    return after + 1;
}


//==============================================================================
void SequenceView::paint (juce::Graphics& g)
{
    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    if (spans == nullptr || spans->empty())
    {
        g.setColour (errorText.isNotEmpty() ? juce::Colour (0xffe0443a) : textDim);
        g.setFont (juce::FontOptions (fontText));
        g.drawFittedText (errorText.isNotEmpty() ? "Der Ablauf kann nicht gespielt werden: " + errorText
                            : juce::String ("Der Ablauf ist leer. Bausteine aus \"Finden\" oder der Ablage hierher ziehen,\noder einen Song laden."_u),
                          getLocalBounds().reduced (12), juce::Justification::centred, 3);
        return;
    }

    for (const auto& b : layout.blocks)
    {
        const auto r = blockRect (b).reduced (1.0f, 0.0f);
        const bool isSelected = b.part == selectedPart;
        const bool active = b.contains (position);
        const auto col = partColour (b.colour);

        g.setColour (col.withAlpha (0.85f));
        g.fillRoundedRectangle (r, 5.0f);
        {
            // dark footer: badge and length sit there, clearly set off from the colour above
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, 5.0f);
            g.reduceClipRegion (clip);
            g.setColour (juce::Colour (0xff24242b).withAlpha (0.9f));
            g.fillRect (r.withTop (r.getBottom() - footerH));
        }
        // light frame around every block: the dark footer on the dark panel needs an edge
        g.setColour (juce::Colour (0xffc4c4cc).withAlpha (0.75f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

        if (active)   // the block that sounds right now: unmistakable frame
        {
            g.setColour (playing);
            g.drawRoundedRectangle (r.reduced (1.5f), 5.0f, 3.0f);
        }
        if (isSelected)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r.reduced (active ? 4.5f : 1.0f), 4.0f, 2.0f);
        }

        for (size_t k = b.firstSpan + 1; k < b.endSpan; ++k)   // repeat boundaries
        {
            g.setColour (background.withAlpha (0.75f));
            g.fillRect (juce::Rectangle<float> (rowBounds (b.row).getX() + b.xOf ((*spans)[k].firstBar) - 1.0f, r.getY() + 2.0f, 2.0f, r.getHeight() - 4.0f));
        }

        const auto& first = (*spans)[b.firstSpan];
        juce::String text = first.label;
        if (first.repeats > 1)
            text << juce::String (juce::CharPointer_UTF8 (" \xc3\x97")) << first.repeats;
        g.setColour (textMain);
        g.setFont (juce::FontOptions (b.width < 80.0f ? fontBlockSmall : fontBlock, juce::Font::bold));
        g.drawFittedText (text, r.toNearestInt().reduced (6, 2).withTrimmedBottom ((int) footerH), juce::Justification::centredLeft, 1);

        // bottom line: the badge (same as in Finden and the Ablage) and the length; the source
        // pattern of a renamed block is in the detail row, not here (unreadable on a block anyway)
        const float badge = badgeSize;
        const auto footer = r.withTop (r.getBottom() - footerH);
        theme::drawSourceBadge (g, juce::Rectangle<float> (r.getX() + 7.0f, footer.getCentreY() - badge * 0.5f, badge, badge), b.colour, b.shape);
        const juce::String bottomLine = juce::String (b.bars) + (b.bars == 1 ? " Takt" : " Takte");
        g.setColour (textMain.withAlpha (0.9f));
        g.setFont (juce::FontOptions (fontBlockSmall));
        g.drawFittedText (bottomLine, footer.toNearestInt().withTrimmedLeft ((int) badge + 13).withTrimmedRight (6), juce::Justification::centredLeft, 1);

        // markers: local variant (pencil), newer source in the library (arrows)
        {
            juce::String marks;
            if (first.edited)        marks << juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x8e"));
            if (first.sourceDiffers) marks << juce::String (juce::CharPointer_UTF8 ("\xe2\x86\xbb"));
            if (marks.isNotEmpty())
            {
                g.setColour (textMain.withAlpha (0.9f));
                g.setFont (juce::FontOptions (15.0f));
                g.drawText (marks, r.toNearestInt().reduced (5, 2), juce::Justification::topRight);
            }
        }

    }

    if (insertMark >= 0)   // insertion mark: a bright bar in the gap
    {
        float x = -1.0f, y = 0.0f, h = 0.0f;
        for (auto& b : layout.blocks)
        {
            const auto r = blockRect (b);
            if (b.part == insertMark)            { x = r.getX() - 2.0f; y = r.getY(); h = r.getHeight(); break; }
            if (b.part == insertMark - 1)        { x = r.getRight() - 1.0f; y = r.getY(); h = r.getHeight(); }
        }
        if (x >= 0.0f)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (x - 1.0f, y - 3.0f, 4.0f, h + 6.0f, 2.0f);
        }
    }

    if (const auto* b = layout.blockAt (position); b != nullptr)   // playhead
    {
        const auto row = rowBounds (b->row);
        const float x = row.getX() + b->xOf (position);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRoundedRectangle (x - 5.0f, row.getY() - 3.0f, 10.0f, row.getHeight() + 6.0f, 4.0f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (x - 1.5f, row.getY() - 3.0f, 3.0f, row.getHeight() + 6.0f, 1.5f);
    }
}

//==============================================================================
void SequenceView::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    pressedPart = -1;
    const auto* b = blockAt (e.getPosition());
    if (b == nullptr)
    {
        selectedPart = -1;
        repaint();
        if (onSelect) onSelect (-1);
        return;
    }
    selectedPart = b->part;
    pressedPart = b->part;
    if (e.mods.isPopupMenu())
    {
        repaint();
        if (onSelect) onSelect (b->part);
        if (onContextMenu) onContextMenu (b->part);
        return;
    }
    repaint();
    if (onSelect)
        onSelect (b->part);
}

void SequenceView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || selectedPart < 0 || spans == nullptr)
        return;
    const auto* b = blockAt (e.getMouseDownPosition());
    if (b == nullptr || b->part != selectedPart)
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        container->startDragging (dragSequence (b->part), this, dragImage ((*spans)[b->firstSpan].label, partColour (b->colour)));
    }
}

void SequenceView::mouseUp (const juce::MouseEvent& e)
{
    const int part = pressedPart;
    pressedPart = -1;
    if (dragging || part < 0 || e.mods.isPopupMenu())
        return;
    if (const auto* b = blockAt (e.getPosition()); b != nullptr && b->part == part && onClick)
        onClick (part);
}

void SequenceView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (const auto* b = blockAt (e.getPosition()); b != nullptr && onDoubleClick)
        onDoubleClick (b->part);
}

bool SequenceView::isInterestedInDragSource (const SourceDetails& d)
{
    const auto s = d.description.toString();
    return s.startsWith ("find:") || s.startsWith ("scratch:") || s.startsWith ("seq:");
}

void SequenceView::itemDragMove (const SourceDetails& d)
{
    const int mark = insertIndexAt (d.localPosition);
    if (mark != insertMark)
    {
        insertMark = mark;
        repaint();
    }
}

void SequenceView::itemDropped (const SourceDetails& d)
{
    const int index = insertIndexAt (d.localPosition);
    insertMark = -1;
    repaint();
    const auto s = d.description.toString();
    if (s.startsWith ("seq:"))
    {
        const int from = s.fromFirstOccurrenceOf (":", false, false).getIntValue();
        // the target index counts positions before removal; after removal everything behind shifts by one
        const int to = index > from ? index - 1 : index;
        if (onMove && to != from)
            onMove (from, to);
    }
    else if (onDrop)
        onDrop (d.description, index);
}
