#include "app/WorkshopWindow.h"

#include "app/Text.h"

namespace
{
    const juce::Colour windowBackground { 0xff121214 };
}

void WorkshopWindow::TabLook::drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool)
{
    const auto area = button.getActiveArea().toFloat();
    const bool front = button.isFrontTab();
    const juce::Colour glow (0xff48a0ff);

    g.setColour (front ? juce::Colour (0xff1c1c20) : isMouseOver && button.isEnabled() ? juce::Colour (0xff17171a) : windowBackground);
    g.fillRect (area);

    if (front)
    {
        // soft glow rising from the bottom edge plus a thin bright line
        g.setGradientFill (juce::ColourGradient (glow.withAlpha (0.0f), area.getCentreX(), area.getY(),
                                                 glow.withAlpha (0.28f), area.getCentreX(), area.getBottom(), false));
        g.fillRect (area);
        g.setColour (glow);
        g.fillRect (area.withTop (area.getBottom() - 2.0f));
    }

    g.setColour (front ? juce::Colours::white : button.isEnabled() ? juce::Colour (0xff8a8a92) : juce::Colour (0xff45454c));
    g.setFont (juce::FontOptions (15.0f, front ? juce::Font::bold : juce::Font::plain));
    g.drawText (button.getButtonText(), area, juce::Justification::centred);
}

WorkshopWindow::WorkshopWindow (EditorHost& h, ArrangerHost& ah, juce::PropertiesFile& p)
    : juce::DocumentWindow ("Werkstatt", windowBackground, juce::DocumentWindow::allButtons),
      host (h), props (p)
{
    setUsingNativeTitleBar (true);

    editor = new PatternEditor (host);
    arranger = new ArrangerPanel (ah);
    tabs.getTabbedButtonBar().setLookAndFeel (&tabLook);
    tabs.setTabBarDepth (32);
    tabs.setOutline (0);
    tabs.addTab ("Raster", windowBackground, editor, true);
    tabs.addTab ("Arranger", windowBackground, arranger, true);

    arranger->onBlockApplied = [this] (const hz::Pattern& applied) { editor->blockApplied (applied); };

    // "Bearbeiten" on a card of the Ablage: as a block, "In Ablage übernehmen" brings it back
    arranger->onEditCardInRaster = [this] (const hz::Pattern& p, const juce::String& title, std::function<bool (const hz::Pattern&)> apply,
                                          std::function<void (const hz::Pattern&)> savedAsNew)
    {
        auto open = [this, p, title, apply, savedAsNew]
        {
            editor->setCardPattern (p, title, apply, savedAsNew);
            showRaster();
        };
        if (! editor->hasUnsavedChanges())
        {
            open();
            return;
        }
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Ungespeicherte Änderungen"_u,
            "Im Raster gibt es ungespeicherte Änderungen an \""_u + juce::String (editor->getPattern().name) + "\". Verwerfen und die Karte laden?",
            "Verwerfen", "Behalten", this,
            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<WorkshopWindow> (this), open] (int r)
            {
                if (safe != nullptr && r == 1)
                    open();
            }));
    };

    // "Bearbeiten" on a block: its copy goes into the raster tab, "In Ablauf übernehmen" brings it back
    arranger->onEditInRaster = [this] (const hz::Pattern& p, const juce::String& title, std::function<bool (const hz::Pattern&)> apply)
    {
        auto open = [this, p, title, apply]
        {
            editor->setBlockPattern (p, title, apply);
            showRaster();
        };
        if (! editor->hasUnsavedChanges())
        {
            open();
            return;
        }
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Ungespeicherte Änderungen"_u,
            "Im Raster gibt es ungespeicherte Änderungen an \""_u + juce::String (editor->getPattern().name) + "\". Verwerfen und den Block laden?",
            "Verwerfen", "Behalten", this,
            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<WorkshopWindow> (this), open] (int r)
            {
                if (safe != nullptr && r == 1)
                    open();
            }));
    };

    tabs.onChange = [this] (int index)
    {
        if (index == 0)
        {
            // an empty raster is useless: show the block that is selected in the arranger
            if (editor->getPattern().bars.empty())
                arranger->editSelectedBlock();   // loads and auditions it
            else
                editor->resumeAudition();
            editor->updateWindowTitle();   // the pattern (and "*"), not the song of the arranger tab
        }
        else
        {
            // the raster's pattern must not keep looping while the arranger is in use
            editor->releaseAudition();
            updateTitle();
            arranger->grabKeyboardFocus();
        }
    };

    setContentNonOwned (&tabs, false);
    Component::addAndMakeVisible (activeGlow);   // on top of the content, see resized()
    setResizable (true, true);
    setResizeLimits (900, 590, 4000, 3000);

    const auto state = props.getValue ("editorWindow");
    if (state.isEmpty() || ! restoreWindowStateFromString (state))
        centreWithSize (1200, 750);
    setVisible (true);

    updateRasterTab();
    updateTitle();
    stateCheck.startTimer (250);
}

void WorkshopWindow::updateTitle()
{
    // the arranger tab names the song; the raster tab names its pattern (PatternEditor::updateWindowTitle)
    if (tabs.getCurrentTabIndex() != 1)
        return;
    const auto title = arranger->songName().isEmpty() ? juce::String ("Werkstatt") : "Werkstatt: " + arranger->songName();
    if (getName() != title)
        setName (title);
}

void WorkshopWindow::updateRasterTab()
{
    // only with a loaded pattern: after "Schließen" the empty raster may stay open, but once left it
    // cannot be clicked again (it would only show an empty field)
    auto* button = tabs.getTabbedButtonBar().getTabButton (0);
    const bool canEdit = ! editor->getPattern().bars.empty();
    if (button == nullptr || button->isEnabled() == canEdit)
        return;
    button->setEnabled (canEdit);
    button->setTooltip (canEdit ? juce::String()
                                : "Erst etwas zum Bearbeiten wählen: im Hauptfenster ein Pattern und \"Bearbeiten\" (E), "
                                  "oder im Arranger einen Block und \"Raster\""_u);
}

WorkshopWindow::~WorkshopWindow()
{
    stateCheck.stopTimer();
    props.setValue ("editorWindow", getWindowStateAsString());
    tabs.getTabbedButtonBar().setLookAndFeel (nullptr);
    clearContentComponent();
}

bool WorkshopWindow::keyPressed (const juce::KeyPress& key)
{
    // With nothing focused inside (right after opening, after a click into the Ablage) key presses
    // land here instead of in the tab's content. Hand them on, so Ctrl+K, Esc, Space & co. always work.
    if (tabs.getCurrentTabIndex() == 1)
        return arranger->keyPressed (key);
    return editor->keyPressed (key);
}

void WorkshopWindow::resized()
{
    juce::DocumentWindow::resized();
    activeGlow.setBounds (getLocalBounds().withHeight (ActiveGlow::height));
    activeGlow.toFront (false);
}

void WorkshopWindow::closeButtonPressed()
{
    // the arranger keeps its working state on disk, so only the raster editor can lose work
    if (! editor->hasUnsavedChanges())
    {
        host.editorClosed();
        return;
    }

    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Ungespeicherte Änderungen"_u,
        "Die Änderungen an \""_u + juce::String (editor->getPattern().name) + "\" sind nicht gespeichert. Trotzdem schließen?"_u,
        "Verwerfen", "Weiter bearbeiten", this,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<WorkshopWindow> (this)] (int r)
        {
            if (safe != nullptr && r == 1)
                safe->host.editorClosed();
        }));
}
