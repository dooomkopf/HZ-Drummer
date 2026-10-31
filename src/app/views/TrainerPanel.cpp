#include "app/views/TrainerPanel.h"

#include "app/Text.h"
#include "app/views/Theme.h"

// One save slot: its number, its name (or "leer"), a small box with range and tempo span, × to clear it
class TrainerPanel::SlotRow : public juce::Component
{
public:
    std::function<void()> onClick, onMenu, onClear;
    std::function<void (const juce::String&)> onRenamed;

    explicit SlotRow (int index)
    {
        addAndMakeVisible (number);
        number.setText (juce::String (index + 1) + ".", juce::dontSendNotification);
        number.setBorderSize ({ 1, 2, 1, 2 });   // a label keeps 5 px either side by default: "10." would be cut
        number.setColour (juce::Label::textColourId, theme::textDim);
        addAndMakeVisible (name);
        name.setColour (juce::Label::backgroundColourId, juce::Colour (0xff26262c));
        name.setColour (juce::Label::outlineColourId, juce::Colour (0xff3a3a42));
        name.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        name.onUp = [this] (const juce::MouseEvent& e)
        {
            if (e.mods.isPopupMenu())
            {
                if (onMenu) onMenu();
                return;
            }
            if (e.mouseWasDraggedSinceMouseDown())
                return;
            if (e.getNumberOfClicks() > 1)
            {
                ++clickToken;   // a double click renames (the label does that), it does not load
                return;
            }
            if (! view.used)
            {
                if (onClick) onClick();   // a free slot: stored at once
                return;
            }
            // a taken slot: loaded once it is clear that no double click follows
            const int token = ++clickToken;
            juce::Timer::callAfterDelay (juce::MouseEvent::getDoubleClickTimeout() + 20,
                                         [safe = juce::Component::SafePointer<SlotRow> (this), token]
            {
                if (safe != nullptr && safe->clickToken == token && safe->onClick)
                    safe->onClick();
            });
        };
        name.onTextChange = [this] { if (onRenamed) onRenamed (name.getText()); };
        addChildComponent (clear);
        clear.setTooltip ("Platz leeren");
        clear.setWantsKeyboardFocus (false);
        clear.onClick = [this] { if (onClear) onClear(); };
    }

    void show (const SlotView& v, bool isActive)
    {
        view = v;
        active = isActive && v.used;
        name.setText (! v.used ? juce::String ("leer") : v.available ? v.name : v.name + "  (fehlt)", juce::dontSendNotification);
        name.setColour (juce::Label::textColourId, v.used && v.available ? theme::textMain : theme::textDim);
        name.setColour (juce::Label::outlineColourId, active ? loopYellow : juce::Colour (0xff3a3a42));
        name.setColour (juce::Label::backgroundColourId, active ? juce::Colour (0xff35321f) : juce::Colour (0xff26262c));
        name.setFont (juce::FontOptions (15.0f, active ? juce::Font::bold : juce::Font::plain));
        name.setEditable (false, v.used, false);   // double click renames a taken slot
        name.setTooltip (! v.used ? "Klick: die jetzige Einstellung (Auswahl, Loop-Bereich, Trainer-Werte) hier ablegen"_u
                         : v.available ? "Klick: laden · Doppelklick: umbenennen · Rechtsklick: überschreiben oder leeren"_u
                                       : "Der gespeicherte Baustein ist nicht mehr da · Rechtsklick: überschreiben oder leeren"_u);
        clear.setVisible (v.used);
        repaint();
    }

    void startRename()
    {
        name.showEditor();
        if (auto* editor = name.getCurrentTextEditor())
            editor->selectAll();
    }

    void paint (juce::Graphics& g) override
    {
        if (active)   // the setup in effect: a yellow frame, as the loop range in the strip
        {
            g.setColour (loopYellow);
            g.drawRoundedRectangle (name.getBounds().toFloat().expanded (1.5f), 3.0f, 2.0f);
        }
        if (! view.used)
            return;
        // the small box: range on top, tempo span below, so that both show whatever the name says
        const auto box = info.toFloat();
        g.setColour (juce::Colour (0xff1f1f25));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (active ? loopYellow : juce::Colour (0xff3a3a42));
        g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (view.available ? theme::textMain : theme::textDim);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (view.rangeText, info.withHeight (info.getHeight() / 2).withTrimmedTop (1), juce::Justification::centred, false);
        g.setColour (theme::textDim);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (view.speedText, info.withTrimmedTop (info.getHeight() / 2).withTrimmedBottom (1), juce::Justification::centred, false);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 2);
        number.setBounds (r.removeFromLeft (32));   // room for "10."
        clear.setBounds (r.removeFromRight (38));   // the look keeps 10 px either side of the text
        r.removeFromRight (8);                       // a little room before ×
        info = r.removeFromRight (74);
        r.removeFromRight (6);
        name.setBounds (r);
    }

private:
    struct NameLabel : juce::Label
    {
        std::function<void (const juce::MouseEvent&)> onUp;
        void mouseUp (const juce::MouseEvent& e) override
        {
            juce::Label::mouseUp (e);
            if (onUp) onUp (e);
        }
    };
    static inline const juce::Colour loopYellow { 0xffe8c547 };   // as the loop range and its button
    juce::Label number;
    NameLabel name;
    juce::TextButton clear { juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) };
    juce::Rectangle<int> info;
    SlotView view;
    bool active = false;
    int clickToken = 0;   // a click waits for a possible double click; a newer click or the double click voids it
};

TrainerPanel::TrainerPanel (const Values& v)
{
    addAndMakeVisible (active);
    active.setButtonText ("Trainer an");
    active.setColour (juce::ToggleButton::textColourId, theme::textMain);
    active.setColour (juce::ToggleButton::tickColourId, theme::textMain);
    active.setToggleState (v.on, juce::dontSendNotification);
    active.onClick = [this] { changed(); };
    setup (from, fromLabel, "Von", 30.0, 320.0, v.from, " BPM");
    setup (to, toLabel, "Bis", 30.0, 320.0, v.to, " BPM");   // the same scale: "Von" sits left of "Bis"
    setup (step, stepLabel, "Inkrement", 1.0, 40.0, v.step, " BPM");
    setup (repeats, repeatsLabel, "Wdh. pro Inkrement", 1.0, 5.0, (double) v.repeats, "");
    from.onValueChange = [this]
    {
        if (from.getValue() > to.getValue() - 1.0)
            from.setValue (to.getValue() - 1.0, juce::dontSendNotification);
        changed();
    };
    to.onValueChange = [this]
    {
        if (to.getValue() < from.getValue() + 1.0)
            to.setValue (from.getValue() + 1.0, juce::dontSendNotification);
        changed();
    };

    addAndMakeVisible (slotsLabel);
    slotsLabel.setText ("Speicherplätze"_u, juce::dontSendNotification);
    slotsLabel.setColour (juce::Label::textColourId, theme::textDim);
    for (int i = 0; i < numSlots; ++i)
    {
        auto& row = rows[(size_t) i];
        row = std::make_unique<SlotRow> (i);
        addAndMakeVisible (*row);
        row->onClick = [this, i] { slotClicked (i); };
        row->onMenu = [this, i] { showSlotMenu (i); };
        row->onClear = [this, i]
        {
            if (onClearSlot) onClearSlot (i);
            slots[(size_t) i] = {};
            setActiveSlot (activeSlot == i ? -1 : activeSlot);
        };
        row->onRenamed = [this, i] (const juce::String& text)
        {
            auto& s = slots[(size_t) i];
            if (text.trim().isNotEmpty())
                s.name = text.trim();
            if (onRenameSlot) onRenameSlot (i, s.name);
            rows[(size_t) i]->show (s, activeSlot == i);
        };
        row->show (slots[(size_t) i], false);
    }
    setSize (480, 12 + 24 + numSlots * 34 + 12 + 14 + 30 + 4 * 32 + 12);
}

TrainerPanel::~TrainerPanel() = default;

void TrainerPanel::setSlots (const std::vector<SlotView>& views)
{
    for (int i = 0; i < numSlots && i < (int) views.size(); ++i)
    {
        slots[(size_t) i] = views[(size_t) i];
        rows[(size_t) i]->show (slots[(size_t) i], activeSlot == i);
    }
}

void TrainerPanel::setActiveSlot (int slot)
{
    activeSlot = slot;
    for (int i = 0; i < numSlots; ++i)
        rows[(size_t) i]->show (slots[(size_t) i], activeSlot == i);
}

void TrainerPanel::paint (juce::Graphics& g)
{
    // the slots' own field
    const auto field = slotsField.toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (field, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (field.reduced (0.5f), 6.0f, 1.0f);
}

void TrainerPanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    slotsLabel.setBounds (r.removeFromTop (24));
    slotsField = r.removeFromTop (numSlots * 34 + 12);
    auto inside = slotsField.reduced (8, 6);
    for (auto& row : rows)
        row->setBounds (inside.removeFromTop (34));
    r.removeFromTop (14);
    active.setBounds (r.removeFromTop (30));
    for (auto [label, slider] : { std::pair { &fromLabel, &from }, std::pair { &toLabel, &to },
                                  std::pair { &stepLabel, &step }, std::pair { &repeatsLabel, &repeats } })
    {
        auto row = r.removeFromTop (32).reduced (0, 3);
        label->setBounds (row.removeFromLeft (136));
        slider->setBounds (row);   // wider now: the menu grew
    }
}

void TrainerPanel::setup (juce::Slider& s, juce::Label& l, const juce::String& text, double lo, double hi, double value, const juce::String& suffix)
{
    addAndMakeVisible (l);
    l.setText (text, juce::dontSendNotification);
    l.setColour (juce::Label::textColourId, theme::textDim);
    addAndMakeVisible (s);
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 24);
    s.setRange (lo, hi, 1.0);
    s.setValue (value, juce::dontSendNotification);
    s.setTextValueSuffix (suffix);
    s.onValueChange = [this] { changed(); };
}

void TrainerPanel::changed()
{
    if (onChange)
        onChange ({ active.getToggleState(), from.getValue(), to.getValue(), step.getValue(), (int) repeats.getValue() });
}

void TrainerPanel::showValues (const Values& v)
{
    active.setToggleState (v.on, juce::dontSendNotification);
    to.setValue (v.to, juce::dontSendNotification);
    from.setValue (v.from, juce::dontSendNotification);
    step.setValue (v.step, juce::dontSendNotification);
    repeats.setValue ((double) v.repeats, juce::dontSendNotification);
}

void TrainerPanel::slotClicked (int slot)
{
    auto& s = slots[(size_t) slot];
    if (! s.used)
    {
        // a free slot: exactly the present setup goes there, with a name offered to type over
        if (! onStoreSlot)
            return;
        s = onStoreSlot (slot);
        setActiveSlot (s.used ? slot : activeSlot);
        if (s.used)
            rows[(size_t) slot]->startRename();
        return;
    }
    if (s.available && onLoadSlot)
        if (const auto values = onLoadSlot (slot))
        {
            showValues (*values);
            setActiveSlot (slot);
        }
}

void TrainerPanel::showSlotMenu (int slot)
{
    if (! slots[(size_t) slot].used)
        return;
    juce::PopupMenu menu;
    menu.addItem (1, "Umbenennen");
    menu.addItem (2, "Mit der jetzigen Einstellung überschreiben"_u);
    menu.addItem (3, "Leeren");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (rows[(size_t) slot].get()),
                        [safe = juce::Component::SafePointer<TrainerPanel> (this), slot] (int result)
    {
        if (safe == nullptr)
            return;
        auto& row = *safe->rows[(size_t) slot];
        if (result == 1)
            row.startRename();
        else if (result == 2 && safe->onStoreSlot)
        {
            safe->slots[(size_t) slot] = safe->onStoreSlot (slot);
            safe->setActiveSlot (slot);
            row.startRename();
        }
        else if (result == 3)
        {
            if (safe->onClearSlot) safe->onClearSlot (slot);
            safe->slots[(size_t) slot] = {};
            safe->setActiveSlot (safe->activeSlot == slot ? -1 : safe->activeSlot);
        }
    });
}
