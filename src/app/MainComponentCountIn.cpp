// MainComponent, part 4: the count-in of the main player. A bar of clicks in the meter of the
// bar we start at plays first; the selection is queued once the clicks sound and takes over at
// the end of that bar (the engine switches plans at bar ends), continuing where it was.

#include "app/MainComponent.h"

#include "app/Text.h"

void MainComponent::cycleCountIn()
{
    countIn = (CountIn) (((int) countIn + 1) % 3);
    updateCountInButton();
    saveSettings();
}

void MainComponent::updateCountInButton()
{
    // grey "Click" = off, blue "START" = only when starting from the first bar, green "AN" = always
    static const char* const names[] = { "Click", "START", "AN" };
    countInButton.setButtonText (juce::String (juce::CharPointer_UTF8 (names[(int) countIn])));
    countInButton.setColour (juce::TextButton::buttonColourId,
                             countIn == CountIn::Off     ? juce::Colour (0xff34343d)
                             : countIn == CountIn::AtStart ? juce::Colour (0xff2f5f8a)
                                                           : juce::Colour (0xff2e7a4a));
}

bool MainComponent::countInWillClick() const
{
    // main player only: not while the raster, the arranger or a menu preview plays something else
    return countIn != CountIn::Off && audition == Audition::None && ! arranging.has_value() && patternMode();
}

bool MainComponent::startWithCountIn()
{
    if (! countInWillClick())
        return false;
    const auto* info = view.plan;   // the selection, or the plan we are paused in
    if (info == nullptr || info->bars.empty())
        return false;
    const int bar = hasShown ? juce::jlimit (0, (int) info->bars.size() - 1, shown.bar) : 0;
    if (countIn == CountIn::AtStart && bar > 0)
        return false;

    auto plan = makeMetronomePlan (info->bars[(size_t) bar].meter, MetronomeSound::Click);
    plan->bars.front().click = true;   // clicks although the selection is a pattern, not the metronome
    PlanInfo countInfo;
    countInfo.title = "Einzählen"_u;
    countInfo.patternId = "countin";
    plans.registerPlan (*plan, meters, std::move (countInfo));
    countInTag = plans.latestTag();
    countInResume = info->bars[(size_t) bar].start;
    countInQueued = false;
    engine.sendPlan (std::move (plan));

    control.play = true;
    ++control.generation;
    publishControl();
    pendingDisplay.clear();
    hasShown = false;
    updateView();
    return true;
}

void MainComponent::followCountIn()
{
    if (countInTag < 0)
        return;
    if (shown.planTag == countInTag && ! countInQueued)
    {
        sendCurrentPlan();            // pending from now on: takes over at the end of the click bar ...
        engine.seek (countInResume);  // ... and starts where the selection was
        countInQueued = true;
    }
    else if (shown.planTag != countInTag && countInQueued)
        countInTag = -1;              // the selection plays
}
