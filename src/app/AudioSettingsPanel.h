#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// System: driver, output, sample rate and buffer size, plus a test tone. The drummer is stereo
// and always plays on the device's first two channels, so the channel list of JUCE's selector is
// left out on purpose (the PulseAudio bridge offers 32 channels, of which only 1 and 2 are real).
class AudioSettingsPanel : public juce::Component,
                           private juce::ChangeListener
{
public:
    explicit AudioSettingsPanel (juce::AudioDeviceManager&);
    ~AudioSettingsPanel() override;

    static constexpr int preferredHeight = 4 * 38 + 24 + 12;   // four rows, the status line, margins

    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override   { refresh(); }
    void refresh();
    void apply (const std::function<void (juce::AudioDeviceManager::AudioDeviceSetup&)>& change);

    juce::AudioDeviceManager& manager;
    juce::Label typeLabel, deviceLabel, rateLabel, bufferLabel, status;
    juce::ComboBox typeBox, deviceBox, rateBox, bufferBox;
    juce::TextButton testButton { "Test" };
    juce::String lastError;
};
