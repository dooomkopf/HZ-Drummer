#include "app/arranger/ScratchView.h"

#include "app/Text.h"
#include "app/arranger/ArrangerStyle.h"
#include "engine/MeterCatalog.h"

using namespace arrangerstyle;

namespace
{
    constexpr int removeSize = 24;             // the "x" square, top right of a card
    constexpr int addW = 86, addH = 22;        // "+ Ablauf", bottom right of a card
}

ScratchView::ScratchView() : content (*this)
{
    addAndMakeVisible (header);
    header.setFont (juce::FontOptions (fontBlockSmall));
    header.setColour (juce::Label::textColourId, textDim);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    setParts ({}, {});
}

void ScratchView::setParts (const std::vector<SongPart>& parts, const std::vector<bool>& used)
{
    cards.clear();
    for (size_t i = 0; i < parts.size(); ++i)
    {
        const auto& p = parts[i];
        Card c;
        c.kind = p.kind;
        c.used = i < used.size() && used[i];
        c.colour = partColourIndex (p);
        c.shape = partShapeIndex (p);
        juce::String err;
        if (partPattern (p, {}, c.played, err))
        {
            c.title = partTitle (p, c.played);
            const int bars = (int) c.played.bars.size();
            c.detail = (c.played.bars.empty() ? juce::String() : juce::String (hz::meterLabel (c.played.bars.front().meter)))
                     + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + juce::String (bars) + (bars == 1 ? " Takt" : " Takte");
        }
        else
        {
            c.title = juce::String (p.sourceId);
            c.detail = "fehlt: " + err;
        }
        cards.push_back (std::move (c));
    }
    int inUse = 0;
    for (auto& c : cards)
        inUse += c.used ? 1 : 0;
    header.setText ("ABLAGE  (" + juce::String ((int) cards.size()) + (inUse > 0 ? ", " + juce::String (inUse) + " im Ablauf" : juce::String()) + ")",
                    juce::dontSendNotification);
    layoutCards();
    content.repaint();
}

int ScratchView::columns() const
{
    return juce::jmax (1, (viewport.getMaximumVisibleWidth() + cardGap) / (cardW + cardGap));
}

juce::Rectangle<int> ScratchView::cardBounds (int index) const
{
    const int cols = columns();
    return { (index % cols) * (cardW + cardGap), (index / cols) * (cardH + cardGap), cardW, cardH };
}

int ScratchView::cardAt (juce::Point<int> p) const
{
    for (int i = 0; i < (int) cards.size(); ++i)
        if (cardBounds (i).contains (p))
            return i;
    return -1;
}

void ScratchView::layoutCards()
{
    const int rows = cards.empty() ? 1 : ((int) cards.size() + columns() - 1) / columns();
    content.setSize (juce::jmax (10, viewport.getMaximumVisibleWidth()), rows * (cardH + cardGap));
}

void ScratchView::resized()
{
    auto area = getLocalBounds();
    header.setBounds (area.removeFromTop (26));
    area.removeFromTop (4);
    viewport.setBounds (area);
    layoutCards();
}

void ScratchView::paint (juce::Graphics& g)
{
    g.setColour (dropHover ? panel.brighter (0.15f) : panel);
    g.fillRoundedRectangle (viewport.getBounds().toFloat().expanded (2.0f), 6.0f);
    if (dropHover)
    {
        g.setColour (accent);
        g.drawRoundedRectangle (viewport.getBounds().toFloat().expanded (2.0f), 6.0f, 2.0f);
    }
    if (cards.empty())
    {
        g.setColour (textDim);
        g.setFont (juce::FontOptions (fontText));
        g.drawFittedText ("Noch leer. Treffer aus \"Finden\" hierher ziehen oder mit \"+ Ablage\" sammeln.\nDie Ablage bleibt erhalten, bis du sie leerst."_u,
                          viewport.getBounds().reduced (12), juce::Justification::centred, 2);
    }
}

bool ScratchView::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("find:");
}

void ScratchView::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    repaint();
    if (onDrop)
        onDrop (d.description);
}

//==============================================================================
void ScratchView::Content::paint (juce::Graphics& g)
{
    auto& o = owner;
    for (int i = 0; i < (int) o.cards.size(); ++i)
    {
        const auto& c = o.cards[(size_t) i];
        const auto r = o.cardBounds (i);
        const bool sounding = o.isAuditioning && o.isAuditioning (i);

        g.setColour (i == hover ? cardHover : card);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        if (c.used)   // part of the sequence: clear green frame
        {
            g.setColour (inSequence);
            g.drawRoundedRectangle (r.toFloat().reduced (1.5f), 6.0f, 3.0f);
        }
        if (sounding)
        {
            g.setColour (playing);
            g.drawRoundedRectangle (r.toFloat().reduced (c.used ? 4.5f : 1.0f), 5.0f, 1.5f);
        }

        // the badge (colour + mark) at the bottom left
        theme::drawSourceBadge (g, juce::Rectangle<float> ((float) r.getX() + 8.0f, (float) r.getBottom() - badgeSize - 6.0f, badgeSize, badgeSize), c.colour, c.shape);

        auto inner = r.reduced (8, 6);
        g.setColour (sounding ? playing : textMain);
        g.setFont (juce::FontOptions (fontBlock - 1.0f));
        g.drawFittedText (c.title, inner.removeFromTop (22).withTrimmedRight (removeSize), juce::Justification::centredLeft, 1);
        g.setColour (textDim);
        g.setFont (juce::FontOptions (fontBlockSmall - 1.0f));
        g.drawFittedText (c.detail, inner.removeFromTop (19), juce::Justification::centredLeft, 1);

        auto bottom = inner.removeFromBottom (addH);
        bottom.removeFromLeft ((int) badgeSize + 4);   // the badge
        if (c.kind == PartKind::Pattern)
            drawMiniPreview (g, c.played, inner.reduced (0, 2).toFloat());
        else
        {
            g.setColour (textDim.withAlpha (0.6f));
            g.setFont (juce::FontOptions (fontSmall - 1.0f));
            g.drawText (c.kind == PartKind::Rest ? "(Stille)" : "(nur Metronom)", inner, juce::Justification::centredLeft);
        }

        // "+ Ablauf" and "x"
        auto add = bottom.removeFromRight (addW);
        g.setColour (i == hover ? accent : accent.withAlpha (0.7f));
        g.setFont (juce::FontOptions (fontBlockSmall));
        g.drawText ("+ Ablauf", add, juce::Justification::centredRight);
        auto rem = juce::Rectangle<int> (r.getRight() - removeSize - 2, r.getY() + 2, removeSize, removeSize);
        g.setColour (i == hover ? juce::Colour (0xff7a2e2e) : juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (rem.toFloat().reduced (3.0f), 4.0f);
        g.setColour (textMain);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")), rem, juce::Justification::centred);
    }
}

void ScratchView::Content::mouseMove (const juce::MouseEvent& e)
{
    const int c = owner.cardAt (e.getPosition());
    if (c != hover)
    {
        hover = c;
        repaint();
    }
}

void ScratchView::Content::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void ScratchView::Content::mouseDown (const juce::MouseEvent& e)
{
    auto& o = owner;
    dragging = false;
    pressedCard = -1;
    if (auto* panel = o.getParentComponent())
        panel->grabKeyboardFocus();   // a click here must not leave the keys with nobody
    const int i = o.cardAt (e.getPosition());
    if (i < 0)
        return;
    const auto r = o.cardBounds (i);
    const auto rem = juce::Rectangle<int> (r.getRight() - removeSize - 2, r.getY() + 2, removeSize, removeSize);
    const auto add = juce::Rectangle<int> (r.getRight() - 8 - addW, r.getBottom() - 6 - addH, addW, addH);
    if (e.mods.isPopupMenu())
    {
        if (o.onContextMenu) o.onContextMenu (i);
    }
    else if (rem.contains (e.getPosition()))
    {
        if (o.onRemove) o.onRemove (i);   // the cards shift: no drag may start from here
    }
    else if (add.contains (e.getPosition()))
    {
        if (o.onToSequence) o.onToSequence (i);
    }
    else
        pressedCard = i;   // the body: drag source, or audition on mouseUp
    repaint();
}

void ScratchView::Content::mouseDrag (const juce::MouseEvent& e)
{
    auto& o = owner;
    if (dragging || pressedCard < 0 || pressedCard >= (int) o.cards.size() || e.getDistanceFromDragStart() < 6)
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        container->startDragging (dragScratch (pressedCard), this, dragImage (o.cards[(size_t) pressedCard].title, selected));
    }
}

void ScratchView::Content::mouseUp (const juce::MouseEvent& e)
{
    auto& o = owner;
    const int i = pressedCard;
    pressedCard = -1;
    if (dragging || i < 0 || o.cardAt (e.getPosition()) != i)
        return;
    if (o.onAudition)
        o.onAudition (i);
    repaint();
}
