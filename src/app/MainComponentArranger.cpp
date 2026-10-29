// MainComponent, part 4: the ArrangerHost interface (auditioning, playing and saving arrangements).

#include "app/MainComponent.h"

#include "app/Text.h"

#include <cmath>

void MainComponent::openArrangerForTesting()
{
    if (editorWindow == nullptr)
        editorWindow = std::make_unique<WorkshopWindow> (*this, *this, *appProperties.getUserSettings());
    editorWindow->showArranger();
    editorWindow->toFront (true);
}

void MainComponent::arrangerAudition (const SongPart& part)
{
    hz::Pattern p;
    juce::String err;
    if (! partPattern (part, library.all(), p, err))
        return;
    arranging.reset();
    editing = p;
    audition = Audition::Arranger;
    menuPreviewIndex = -1;
    sendCurrentPlan();
    if (! control.play)
        togglePlay();
}

void MainComponent::arrangerPlay (const Song& song, int fromPart)
{
    editing.reset();
    audition = Audition::None;
    menuPreviewIndex = -1;
    arranging = song;
    sendCurrentPlan();

    // start at the first bar of the part; the seek lands in the plan just sent (see PatternPlayer::seek)
    if (const auto* info = plans.latest(); info != nullptr && fromPart > 0)
        for (auto& sp : info->spans)
            if (sp.part == fromPart && sp.repeat == 0 && sp.firstBar < (int) info->bars.size())
            {
                engine.seek (info->bars[(size_t) sp.firstBar].start);
                pendingDisplay.clear();
                break;
            }
    if (! control.play)
        togglePlay();
}

void MainComponent::arrangerUpdate (const Song& song)
{
    if (! arranging.has_value())
        return;
    const auto* before = plans.latest();
    const double oldLength = before != nullptr && ! before->bars.empty() ? before->bars.back().start + before->bars.back().length : 0.0;
    const int bar = view.hasPosition() ? view.bar : 0;

    arranging = song;
    sendCurrentPlan();

    // Playing with the same length: the player continues in place by itself. Otherwise, and
    // always while paused (a new plan would start at 0), go on at the same bar (clamped).
    if (const auto* info = plans.latest(); info != nullptr && ! info->bars.empty())
    {
        const double newLength = info->bars.back().start + info->bars.back().length;
        if (! control.play || std::abs (newLength - oldLength) > 1.0e-9)
        {
            const int target = juce::jlimit (0, (int) info->bars.size() - 1, bar);
            engine.seek (info->bars[(size_t) target].start);
            pendingDisplay.clear();
            if (! control.play)
            {
                // the paused marker belongs to the new plan now
                shown.bar = target;
                shown.pulse = 0;
                shown.meter = info->bars[(size_t) target].meterTag;
                shown.planTag = plans.latestTag();
                shown.timeMs = juce::Time::getMillisecondCounterHiRes();
                hasShown = true;
                updateView();
            }
        }
    }
}

void MainComponent::arrangerSeekToPart (int part)
{
    // the plan shown is the arrangement (or the same song): its spans say where the part starts
    const auto* info = view.plan;
    if (info == nullptr)
        return;
    for (auto& sp : info->spans)
        if (sp.part == part && sp.repeat == 0)
        {
            seekToBar (sp.firstBar);
            return;
        }
}

void MainComponent::arrangerStop()
{
    if (control.play)
        togglePlay();
    if (audition == Audition::Arranger)
    {
        editing.reset();
        audition = Audition::None;
    }
    arranging.reset();
    sendCurrentPlan();
}

juce::String MainComponent::arrangerSaveSong (Song& song)
{
    auto folder = Library::songFolder();
    if (! folder.createDirectory())
        return "Ordner nicht anlegbar: "_u + folder.getFullPathName();

    if (song.id.empty())
    {
        juce::String slug;
        for (auto c : juce::String (song.name).toLowerCase())
            slug << (c < 128 && juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : juce::String ("-"));
        slug = slug.trimCharactersAtStart ("-").trimCharactersAtEnd ("-").substring (0, 40);
        do
            song.id = (slug + "-" + juce::Uuid().toString().substring (0, 8)).toStdString();
        while (folder.getChildFile (juce::String (song.id) + ".json").exists());
    }

    juce::String err;
    if (! embedCopies (song, library.all(), err) || ! saveSong (song, folder.getChildFile (juce::String (song.id) + ".json"), err))
        return err;

    // the library lists the song now; keep the main window's selection by id
    const auto selectedId = patternMode() ? library.get (currentPattern).id : std::string();
    library.reload();
    currentPattern = selectedId.empty() ? -1 : library.indexOf (selectedId);
    updateButtons();
    return {};
}
