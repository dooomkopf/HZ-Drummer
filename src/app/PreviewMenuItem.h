#pragma once

#include "app/views/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

// Remember the button at press time: mouse-up modifiers may no longer carry it.
struct MenuClickGesture
{
    enum class Result { None, Select, Context };
    bool primary = false, popup = false;
    void press (juce::ModifierKeys mods)
    {
        popup = mods.isPopupMenu();
        primary = mods.isLeftButtonDown() && ! popup;
    }
    Result release (juce::ModifierKeys mods, bool inside)
    {
        const bool context = popup || mods.isPopupMenu();
        const bool select = primary && ! context && inside;
        primary = popup = false;
        return context ? Result::Context : select ? Result::Select : Result::None;
    }
};

// A pattern entry of the main window's pattern menu that can be auditioned while the menu
// stays open: hover it and press P (again: stop). Clicking or Enter selects it as usual.
// The item takes the keyboard focus when the mouse enters it or the arrow keys highlight it,
// so P reaches it; every other key goes on to the menu (arrows, Enter, Esc).
class PreviewMenuItem : public juce::PopupMenu::CustomComponent
{
public:
    PreviewMenuItem (juce::String textToShow, juce::Colour textColour, bool isTicked,
                     std::function<void()> preview, std::function<bool()> isPreviewing,
                     std::shared_ptr<bool> selectionAllowed = {})
        : juce::PopupMenu::CustomComponent (false),
          text (std::move (textToShow)), colour (textColour), ticked (isTicked),
          onPreview (std::move (preview)), previewing (std::move (isPreviewing)), allowSelection (std::move (selectionAllowed))
    {
        setWantsKeyboardFocus (true);
    }

    void getIdealSize (int& w, int& h) override
    {
        w = 60 + (int) juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (fontSize)), text);
        h = 26;
    }

    void paint (juce::Graphics& g) override
    {
        const bool highlighted = isItemHighlighted();
        const bool sounding = previewing && previewing();
        if (highlighted)
        {
            g.setColour (juce::Colour (0xff2f4f7a));
            g.fillRect (getLocalBounds());
            if (! hasKeyboardFocus (false))   // arrow keys moved the highlight here: let P reach us
                juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PreviewMenuItem> (this)]
                {
                    if (safe != nullptr && safe->isItemHighlighted())
                        safe->grabKeyboardFocus();
                });
        }

        auto r = getLocalBounds().reduced (6, 0);
        auto mark = r.removeFromLeft (20);
        g.setFont (juce::FontOptions (fontSize));
        if (sounding)
        {
            g.setColour (juce::Colour (0xff3cc6b4));
            juce::Path tri;
            tri.addTriangle ((float) mark.getX() + 4.0f, (float) mark.getCentreY() - 6.0f, (float) mark.getX() + 4.0f, (float) mark.getCentreY() + 6.0f,
                             (float) mark.getX() + 14.0f, (float) mark.getCentreY());
            g.fillPath (tri);
        }
        else if (ticked)
        {
            g.setColour (theme::textMain);
            g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93")), mark, juce::Justification::centred);
        }
        g.setColour (sounding ? juce::Colour (0xff3cc6b4) : colour);
        g.drawFittedText (text, r, juce::Justification::centredLeft, 1);
    }

    void mouseEnter (const juce::MouseEvent&) override { grabKeyboardFocus(); repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        gesture.press (e.mods);
        if (allowSelection) *allowSelection = false;
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto result = gesture.release (e.mods, getLocalBounds().contains (e.getPosition()));
        if (result == MenuClickGesture::Result::Context)
            return;   // right click: no selection, no dismissal and no context menu
        if (result == MenuClickGesture::Result::Select)
        {
            if (allowSelection) *allowSelection = true;
            triggerMenuItem();
        }
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::returnKey)
        {
            if (allowSelection) *allowSelection = true;
            triggerMenuItem();
            return true;
        }
        if (! key.getModifiers().isAnyModifierKeyDown() && juce::CharacterFunctions::toLowerCase (key.getTextCharacter()) == 'p')
        {
            if (onPreview)
                onPreview();
            // the entry that was auditioned before must lose its mark too: it may sit in another
            // (sub)menu window, so repaint every window on the desktop
            auto& desktop = juce::Desktop::getInstance();
            for (int i = 0; i < desktop.getNumComponents(); ++i)
                if (auto* c = desktop.getComponent (i))
                    c->repaint();
            return true;
        }
        return false;   // arrows, Enter, Esc: the menu handles them
    }

private:
    static constexpr float fontSize = 16.0f;

    juce::String text;
    juce::Colour colour;
    bool ticked;
    std::function<void()> onPreview;
    std::function<bool()> previewing;
    std::shared_ptr<bool> allowSelection;
    MenuClickGesture gesture;
};
