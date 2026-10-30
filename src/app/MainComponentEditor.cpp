// MainComponent, part 3: the workshop window (raster editor) and the EditorHost interface.

#include "app/MainComponent.h"

#include "app/Text.h"

void MainComponent::requestQuit()
{
    // the Werkstatt's own X asks about unsaved raster changes; quitting the whole program must too
    if (editorWindow == nullptr || ! editorWindow->getEditor().hasUnsavedChanges())
    {
        juce::JUCEApplication::quit();
        return;
    }
    if (quitQuestionOpen)
        return;
    quitQuestionOpen = true;
    editorWindow->toFront (true);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Ungespeicherte Änderungen"_u,
        "Die Änderungen an \""_u + juce::String (editorWindow->getEditor().getPattern().name) + "\" sind nicht gespeichert. Trotzdem beenden?"_u,
        "Verwerfen", "Weiter bearbeiten", editorWindow.get(),
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainComponent> (this)] (int r)
        {
            if (safe == nullptr)
                return;
            safe->quitQuestionOpen = false;
            if (r == 1)
                juce::JUCEApplication::quit();
        }));
}

bool MainComponent::patternNameTaken (const juce::String& name) const
{
    for (int i = 0; i < library.size(); ++i)
        if (! library.isSong (i) && juce::String (library.get (i).name) == name)
            return true;
    return false;
}

void MainComponent::openEditor()
{
    // a song goes to the arranger, a pattern to the raster editor
    if (library.isSong (currentPattern))
    {
        if (editorWindow == nullptr)
            editorWindow = std::make_unique<WorkshopWindow> (*this, *this, *appProperties.getUserSettings());
        if (const auto* song = library.song (currentPattern))
            editorWindow->getArranger().loadSong (*song);
        editorWindow->showArranger();
        editorWindow->toFront (true);
        return;
    }

    // already open: bring it to the front and load the selected pattern (asking about unsaved changes)
    if (editorWindow != nullptr)
    {
        if (patternMode())
            loadIntoEditor (currentPattern);
        editorWindow->showRaster();
        editorWindow->toFront (true);
        return;
    }

    editorWindow = std::make_unique<WorkshopWindow> (*this, *this, *appProperties.getUserSettings());

    if (patternMode())
    {
        editorWindow->getEditor().setPattern (library.get (currentPattern), library.isUserPattern (currentPattern));
    }
    else
    {
        // new pattern in the current meter
        hz::Pattern p;
        p.name = "Neues Pattern";
        p.tempo = control.tempoTenths / 10.0;
        hz::PatternBar bar;
        bar.meter = meters.get (currentMeter);
        bar.grid = hz::Rational (1, bar.meter.denominator >= 16 ? 16 : bar.meter.denominator >= 8 ? 8 : 16);
        p.bars.push_back (bar);
        p.layers = { { 1, hz::LayerRole::Hands, {} }, { 2, hz::LayerRole::Feet, {} } };
        p.humanizeSeed = (std::uint32_t) juce::Random::getSystemRandom().nextInt() | 1u;
        editorWindow->getEditor().setPattern (p, false);
    }
    editorWindow->toFront (true);
}

void MainComponent::loadIntoEditor (int index)
{
    if (editorWindow == nullptr || index < 0)
        return;

    auto& ed = editorWindow->getEditor();
    if (! ed.hasUnsavedChanges())
    {
        ed.setPattern (library.get (index), library.isUserPattern (index));
        return;
    }

    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Ungespeicherte Änderungen"_u,
        "Im Editor gibt es ungespeicherte Änderungen an \""_u + juce::String (ed.getPattern().name) + "\". Verwerfen und \""
            + juce::String (library.get (index).name) + "\" laden?",
        "Verwerfen", "Behalten", this,
        juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainComponent> (this), id = library.get (index).id] (int r)
        {
            // look it up by id: the library may have changed meanwhile
            if (safe == nullptr || r != 1 || safe->editorWindow == nullptr)
                return;
            if (const int i = safe->library.indexOf (id); i >= 0)
                safe->editorWindow->getEditor().setPattern (safe->library.get (i), safe->library.isUserPattern (i));
        }));
}

void MainComponent::auditionPattern (const hz::Pattern& p)
{
    // the raster's pattern plays while its tab is in front; 'arranging' stays and returns on release
    editing = p;
    audition = Audition::Raster;
    menuPreviewIndex = -1;
    sendCurrentPlan();
}

void MainComponent::auditionStopped()
{
    if (audition != Audition::Raster)
        return;   // only the raster's own audition is released here
    editing.reset();
    audition = Audition::None;
    sendCurrentPlan();
    updateButtons();
}

juce::String MainComponent::savePattern (hz::Pattern& p, bool asNew)
{
    if (auto err = library.save (p, asNew); err.isNotEmpty())
        return err;

    currentPattern = library.indexOf (p.id);
    editing = p;
    sendCurrentPlan();
    updateButtons();
    saveSettings();
    if (editorWindow != nullptr)
        editorWindow->getArranger().libraryChanged();
    return {};
}

juce::String MainComponent::deletePattern (const hz::Pattern& p)
{
    // the raster clears itself afterwards; the workshop window stays open
    return removeLibraryEntry (p.id);
}

EditorHost::Playhead MainComponent::getPlayhead() const
{
    Playhead ph;
    const auto* info = plans.find (shown.planTag);
    if (control.play && hasShown && info != nullptr && info->fromEditor)
    {
        ph.playing = true;
        ph.bar = shown.bar;
        ph.pulse = shown.pulse;
    }
    return ph;
}

bool MainComponent::handleTransportKey (const juce::KeyPress& key)
{
    // in the workshop only transport keys work (left/right would switch the pattern). Space never
    // starts the main window's selection there (nor its count-in): it pauses what sounds and
    // resumes only what the workshop put into the transport
    if (! isTransportKey (key))
        return false;
    if (key.getKeyCode() == juce::KeyPress::spaceKey && ! control.play && audition == Audition::None && ! arranging.has_value())
        return true;   // nothing of the workshop's to play: swallow it
    return handleGlobalKey (key);
}

void MainComponent::editorClosed()
{
    // never delete the window from inside its own callback
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safe == nullptr)
            return;
        safe->editorWindow = nullptr;
        safe->editing.reset();
        safe->audition = Audition::None;
        safe->arranging.reset();
        safe->sendCurrentPlan();
        safe->updateButtons();
        safe->saveSettings();
    });
}
