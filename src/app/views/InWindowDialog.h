#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// An AlertWindow shown inside the window it belongs to instead of as a window of its own.
// A separate dialog window can end up behind its parent (seen with Cinnamon and two screens);
// being modal it then blocks the whole program without being visible. Inside the window it
// cannot get lost, and it only blocks that window: the other windows stay usable.
// HZLook::createAlertWindow makes every JUCE message box one of these; dialogs with text fields
// create it directly, with the same arguments as an AlertWindow.
class InWindowDialog : public juce::AlertWindow,
                       private juce::ComponentListener
{
public:
    InWindowDialog (const juce::String& title, const juce::String& message,
                    juce::MessageBoxIconType iconType, juce::Component* associated);
    ~InWindowDialog() override;

    bool canModalEventBeSentToComponent (const juce::Component* target) override;
    bool keyPressed (const juce::KeyPress&) override;
    void parentSizeChanged() override;

private:
    void componentBeingDeleted (juce::Component&) override;

    juce::Component::SafePointer<juce::Component> near, window;
};
