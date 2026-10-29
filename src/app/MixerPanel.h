#pragma once

#include "app/MixState.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

// Mixer window content: per-group volume (dB) with mute (M) and solo (S), a level meter next to
// each fader, and pale faders for channels that do not sound in what is selected
class MixerPanel : public juce::Component,
                   private juce::Timer
{
public:
    using Setting = mix::Setting;

    // takeLevel: peak (linear) per channel since the last call; inUse: does the channel sound at all
    MixerPanel (const mix::Settings& initial, std::function<void (int, Setting)> onChange,
                std::function<float (int)> takeLevel, std::function<bool (int)> inUse);
    ~MixerPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    class FaderLook;

    struct Strip
    {
        juce::Slider slider;
        juce::TextButton mute { "M" }, solo { "S" };
        juce::Label name;
        float meterDb = -100.0f;   // shown level, falls slowly
        bool inUse = true;
    };

    void notify (int ch);
    void timerCallback() override;
    juce::Rectangle<int> meterArea (int ch) const;

    std::unique_ptr<FaderLook> look;
    std::array<Strip, mix::NumChannels> strips;
    std::function<void (int, Setting)> onChange;
    std::function<float (int)> takeLevel;
    std::function<bool (int)> inUse;
    juce::TextButton resetButton { "Reset" };
};
