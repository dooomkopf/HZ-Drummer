// MainComponent, part 6: the speed trainer's save slots. A slot keeps a practice spot under a name: the
// selection, its loop range and the trainer's values (see TrainerSlots.h, TrainerPanel.h).

#include "app/MainComponent.h"

#include "app/Text.h"

static_assert (TrainerPanel::numSlots == trainerslots::count);

void MainComponent::wireTrainerSlots (TrainerPanel& panel)
{
    std::vector<TrainerPanel::SlotView> views;
    for (int i = 0; i < trainerslots::count; ++i)
        views.push_back (trainerSlotView (i));
    panel.setSlots (views);
    panel.setActiveSlot (activeTrainerSlot);

    const juce::Component::SafePointer<MainComponent> safe (this);
    panel.onStoreSlot = [safe] (int i) { return safe != nullptr ? safe->storeTrainerSlot (i) : TrainerPanel::SlotView(); };
    panel.onLoadSlot = [safe] (int i) -> std::optional<TrainerPanel::Values>
    {
        return safe != nullptr ? safe->loadTrainerSlot (i) : std::nullopt;
    };
    panel.onRenameSlot = [safe] (int i, const juce::String& name)
    {
        if (safe == nullptr || name.trim().isEmpty())
            return;
        safe->trainerSlots[(size_t) i].name = name.trim();
        trainerslots::save (safe->trainerSlots, trainerslots::file());
    };
    panel.onClearSlot = [safe] (int i)
    {
        if (safe == nullptr)
            return;
        safe->trainerSlots[(size_t) i] = {};
        if (safe->activeTrainerSlot == i)
            safe->activeTrainerSlot = -1;
        trainerslots::save (safe->trainerSlots, trainerslots::file());
    };
}

TrainerPanel::SlotView MainComponent::trainerSlotView (int slot) const
{
    const auto& s = trainerSlots[(size_t) slot];
    TrainerPanel::SlotView v { s.name, ! s.isFree(), s.isFree() || s.patternId.empty() || library.indexOf (s.patternId) >= 0, {}, {} };
    if (! s.isFree())
    {
        // the small box: bars of the range (or the whole), the tempo span, whatever the name says
        v.rangeText = s.loopFirst < 0 ? juce::String ("ganz")
                    : "T. " + juce::String (s.loopFirst + 1) + (s.loopLast > s.loopFirst ? "-" + juce::String (s.loopLast + 1) : juce::String());
        v.speedText = juce::String ((int) s.from) + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")) + juce::String ((int) s.to);
    }
    return v;
}

TrainerPanel::SlotView MainComponent::storeTrainerSlot (int slot)
{
    // exactly the present setup: what is selected (not a preview), its range, the trainer's values
    TrainerSlot s;
    if (patternMode())
        s.patternId = library.get (currentPattern).id;
    else
        s.meterKey = MeterLibrary::key (meters.get (currentMeter));
    if (loopFirst >= 0)
    {
        s.loopFirst = loopFirst;
        s.loopLast = loopLast;
        s.loopOn = loopOn;
    }
    s.from = trainer.from;
    s.to = trainer.to;
    s.step = trainer.step;
    s.repeats = trainer.repeats;

    // a name to type over: the title (range and tempo span stand in their own box)
    s.name = patternMode() ? juce::String (library.get (currentPattern).name) : "Metronom " + s.meterKey;

    trainerSlots[(size_t) slot] = s;
    activeTrainerSlot = slot;   // this setup is the one in effect
    trainerslots::save (trainerSlots, trainerslots::file());
    return trainerSlotView (slot);
}

std::optional<TrainerPanel::Values> MainComponent::loadTrainerSlot (int slot)
{
    const auto s = trainerSlots[(size_t) slot];   // a copy: selecting saves settings and so on
    if (s.isFree() || workshopOpen())
        return std::nullopt;
    if (! s.patternId.empty())
    {
        const int index = library.indexOf (s.patternId);
        if (index < 0)
            return std::nullopt;   // deleted meanwhile
        selectPattern (index);
    }
    else if (const int meter = meters.fromKey (s.meterKey); meter >= 0)
        selectMeter (meter);
    else
        return std::nullopt;

    // the range comes back (the selection cleared the one before); playback starts at it
    if (const auto* selection = plans.find (selectionTag); selection != nullptr && s.loopFirst >= 0
        && s.loopFirst < (int) selection->bars.size())
    {
        loopFirst = s.loopFirst;
        loopLast = juce::jmin (s.loopLast, (int) selection->bars.size() - 1);
        loopOn = s.loopOn;
        loopPatternId = selection->patternId;
        sendLoop();
        if (loopOn)
        {
            if (control.play)
                engine.seek (selection->bars[(size_t) loopFirst].start);   // the new plan waits for the bar end and starts there
            else
                seekToBar (loopFirst);
        }
    }

    activeTrainerSlot = slot;   // after the selection, which would have cleared it

    // the trainer's values, ready at "Von"; S (or "Trainer an") starts it
    if (trainer.on)
        setTrainerOn (false);
    trainer.from = s.from;
    trainer.to = s.to;
    trainer.step = s.step;
    trainer.repeats = s.repeats;
    setTempo (s.from);
    saveSettings();
    return trainer;
}
