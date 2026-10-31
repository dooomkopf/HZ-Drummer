#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

// The small menu behind the "Trainer" button: on/off, from, to, increment and repeats per increment, each a
// small slider with its value on the right. The speed trainer starts at "Von", goes up by the increment after
// that many passes and, once at "Bis", simply goes on at that tempo. "Von" always stays below "Bis": each of
// the two stops two BPM short of the other; both move in steps of two. A pass is one round through the loop range, else through the
// pattern or arrangement.
//
// Above them, in a field of their own, the save slots: a click on a free one puts exactly the present setup there
// (selection, loop range, trainer values) and offers a name to type over; a click on a taken one brings that
// setup back. A small box before × shows the range and, below it, the tempo span, whatever the name says. The
// slot loaded (or stored) last is highlighted. Double click renames, the right mouse button overwrites or
// clears, × clears.
class TrainerPanel : public juce::Component
{
public:
    struct Values
    {
        bool on = false;
        double from = 100.0, to = 140.0, step = 1.0;
        int repeats = 1;
    };
    struct SlotView
    {
        juce::String name;
        bool used = false;
        bool available = true;   // what it holds is still there (not deleted meanwhile)
        juce::String rangeText, speedText;   // the small box: "T. 5-8" (or "ganz"), "100→140"
    };
    static constexpr int numSlots = 10;

    std::function<void (const Values&)> onChange;
    std::function<SlotView (int)> onStoreSlot;                  // the present setup into slot i: its view, with a name offered
    std::function<std::optional<Values> (int)> onLoadSlot;      // slot i applied: the trainer values it brought
    std::function<void (int, const juce::String&)> onRenameSlot;
    std::function<void (int)> onClearSlot;

    explicit TrainerPanel (const Values&);
    ~TrainerPanel() override;
    void setSlots (const std::vector<SlotView>&);
    void setActiveSlot (int slot);   // -1: none
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class SlotRow;

    void setup (juce::Slider&, juce::Label&, const juce::String& text, double lo, double hi, double value,
                const juce::String& suffix, double step = 1.0);
    void changed();
    void showValues (const Values&);   // without notifying
    void slotClicked (int slot);
    void showSlotMenu (int slot);

    juce::ToggleButton active;
    juce::Slider from, to, step, repeats;
    juce::Label fromLabel, toLabel, stepLabel, repeatsLabel, slotsLabel;
    std::array<std::unique_ptr<SlotRow>, numSlots> rows;
    std::array<SlotView, numSlots> slots;
    int activeSlot = -1;
    juce::Rectangle<int> slotsField;   // the slots' own field
};
