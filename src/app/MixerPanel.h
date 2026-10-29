#pragma once

#include "app/MixState.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

// Mixer window content: per-group volume (dB) with mute (M) and solo (S)
class MixerPanel : public juce::Component
{
public:
    using Setting = mix::Setting;

    MixerPanel (const mix::Settings& initial, std::function<void (int, Setting)> onChange);
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
    };

    void notify (int ch);

    std::unique_ptr<FaderLook> look;
    std::array<Strip, mix::NumChannels> strips;
    std::function<void (int, Setting)> onChange;
    juce::TextButton resetButton { "Reset" };
};
