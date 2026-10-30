#include "app/views/HelpLine.h"

#include "app/Library.h"
#include "app/Text.h"
#include "app/views/InWindowDialog.h"

namespace
{
    const juce::Colour hintRed { 0xffe07a70 };   // as HintMenuItem in NavigationMenus.cpp
}

HelpLine::HelpLine (juce::String name, std::function<juce::String()> fallback, float size, int lines)
    : fileName (std::move (name)), defaultText (std::move (fallback)), fontSize (size), maxLines (lines)
{
    hasOwn = file().existsAsFile();
    ownText = hasOwn ? file().loadFileAsString().trim() : juce::String();

    addAndMakeVisible (editButton);
    editButton.setWantsKeyboardFocus (false);
    editButton.setTooltip ("Eigenen Text für diese Hilfezeile eingeben (Reset stellt den Standardtext wieder her)"_u);
    editButton.onClick = [this] { askText(); };
}

juce::File HelpLine::file() const
{
    return Library::songFolder().getSiblingFile (fileName);
}

juce::String HelpLine::text() const
{
    return hasOwn ? ownText : defaultText();
}

void HelpLine::paint (juce::Graphics& g)
{
    g.setColour (hintRed);
    g.setFont (juce::FontOptions (fontSize));
    g.drawFittedText (text(), getLocalBounds().withTrimmedRight (48), juce::Justification::centredLeft, maxLines);
}

void HelpLine::resized()
{
    editButton.setBounds (getLocalBounds().removeFromRight (40).withSizeKeepingCentre (40, juce::jmin (20, getHeight())));
}

void HelpLine::askText()
{
    auto* w = new InWindowDialog ("Hilfezeile", "Eigener Text für diese Hilfezeile. Leer lassen = keine Hilfezeile.\n"
                                                "Reset stellt den Standardtext wieder her."_u,
                                  juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("text", text().replace ("\n", "  ·  "_u), "Text:");   // one line to edit; long text wraps when drawn
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Reset", 2);
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<HelpLine> (this), w] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        const auto typed = w->getTextEditorContents ("text").trim();
        // the default is not stored: an update of it then reaches everyone who did not write their own
        if (r == 2 || typed == safe->defaultText().replace ("\n", "  ·  "_u))
        {
            safe->file().deleteFile();
            safe->hasOwn = false;
            safe->ownText = {};
        }
        else if (safe->file().replaceWithText (typed))
        {
            safe->hasOwn = true;
            safe->ownText = typed;
        }
        safe->repaint();
    }), true);
}
