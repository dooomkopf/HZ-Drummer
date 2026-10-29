#include "app/views/InWindowDialog.h"

InWindowDialog::InWindowDialog (const juce::String& title, const juce::String& message,
                                juce::MessageBoxIconType iconType, juce::Component* associated)
    : juce::AlertWindow (title, message, iconType, associated),
      near (associated)
{
    auto* top = associated != nullptr ? associated->getTopLevelComponent() : nullptr;
    if (top == nullptr)
        if (auto* active = juce::TopLevelWindow::getActiveTopLevelWindow())
            top = active->getTopLevelComponent();
    if (top == nullptr || top == this || ! top->isShowing())
        return;   // nowhere to go: stays a window of its own

    window = top;
    top->addAndMakeVisible (this);   // also takes it off the desktop
    top->addComponentListener (this);
    parentSizeChanged();
}

InWindowDialog::~InWindowDialog()
{
    if (window != nullptr)
        window->removeComponentListener (this);
}

bool InWindowDialog::canModalEventBeSentToComponent (const juce::Component* target)
{
    if (window == nullptr || target == nullptr)
        return false;   // a dialog window of its own blocks everything, like any JUCE dialog

    // JUCE only asks the topmost dialog, so check them all: a window with a dialog of its own
    // stays blocked, and so does everything while a separate dialog window is open
    const auto* targetWindow = target->getTopLevelComponent();
    auto& modal = *juce::ModalComponentManager::getInstance();
    for (int i = 0; i < modal.getNumModalComponents(); ++i)
    {
        const auto* m = modal.getModalComponent (i);
        if (m == nullptr || m == target || m->isParentOf (target))
            continue;
        const auto* modalWindow = m->getTopLevelComponent();
        if (modalWindow == targetWindow || modalWindow == m)
            return false;
    }
    return true;
}

bool InWindowDialog::keyPressed (const juce::KeyPress& key)
{
    // unused keys would travel on to the window behind the dialog (Space, Ctrl+S, ...);
    // only Tab goes on, JUCE moves the focus between the fields with it
    return juce::AlertWindow::keyPressed (key) || ! key.isKeyCode (juce::KeyPress::tabKey);
}

void InWindowDialog::parentSizeChanged()
{
    if (getParentComponent() != nullptr)
        centreAroundComponent (near != nullptr ? near.getComponent() : getParentComponent(), getWidth(), getHeight());
}

void InWindowDialog::componentBeingDeleted (juce::Component&)
{
    // The window closes with the dialog still open. The modal manager answers "cancel" but no
    // longer deletes a dialog whose parent goes away, and the window does not delete its children.
    window = nullptr;
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<InWindowDialog> (this)]
    {
        delete safe.getComponent();
    });
}
