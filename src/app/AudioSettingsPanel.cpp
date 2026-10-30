#include "app/AudioSettingsPanel.h"

#include "app/Text.h"
#include "app/views/Theme.h"

AudioSettingsPanel::AudioSettingsPanel (juce::AudioDeviceManager& dm) : manager (dm)
{
    const std::pair<juce::Label*, juce::String> labels[] = {
        { &typeLabel, "Treiber" }, { &deviceLabel, "Ausgang" }, { &rateLabel, "Abtastrate" }, { &bufferLabel, "Puffer" } };
    for (auto& [label, text] : labels)
    {
        addAndMakeVisible (*label);
        label->setText (text, juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centredRight);
    }
    for (auto* box : { &typeBox, &deviceBox, &rateBox, &bufferBox })
        addAndMakeVisible (*box);
    addAndMakeVisible (testButton);
    testButton.setTooltip ("Spielt einen kurzen Testton");
    addAndMakeVisible (status);
    status.setColour (juce::Label::textColourId, theme::textDim);

    typeBox.onChange = [this]
    {
        const auto name = typeBox.getText();
        if (name.isEmpty() || name == manager.getCurrentAudioDeviceType())
            return;
        manager.setCurrentAudioDeviceType (name, true);
        if (auto* type = manager.getCurrentDeviceTypeObject())
            type->scanForDevices();
        lastError = manager.getCurrentAudioDevice() == nullptr ? "Mit diesem Treiber ist kein Ausgang geöffnet."_u : juce::String();
        refresh();
    };
    deviceBox.onChange = [this]
    {
        apply ([name = deviceBox.getText()] (auto& setup)
        {
            setup.outputDeviceName = name;
            setup.inputDeviceName = {};
            setup.useDefaultOutputChannels = true;   // the first two
        });
    };
    rateBox.onChange = [this] { apply ([rate = rateBox.getSelectedId()] (auto& setup) { setup.sampleRate = rate; }); };
    bufferBox.onChange = [this] { apply ([size = bufferBox.getSelectedId()] (auto& setup) { setup.bufferSize = size; }); };
    testButton.onClick = [this] { manager.playTestSound(); };

    if (auto* type = manager.getCurrentDeviceTypeObject())
        type->scanForDevices();
    manager.addChangeListener (this);
    refresh();
}

AudioSettingsPanel::~AudioSettingsPanel()
{
    manager.removeChangeListener (this);
}

void AudioSettingsPanel::apply (const std::function<void (juce::AudioDeviceManager::AudioDeviceSetup&)>& change)
{
    auto setup = manager.getAudioDeviceSetup();
    change (setup);
    lastError = manager.setAudioDeviceSetup (setup, true);
    refresh();
}

void AudioSettingsPanel::refresh()
{
    typeBox.clear (juce::dontSendNotification);
    const auto& types = manager.getAvailableDeviceTypes();
    for (int i = 0; i < types.size(); ++i)
        typeBox.addItem (types[i]->getTypeName(), i + 1);
    typeBox.setText (manager.getCurrentAudioDeviceType(), juce::dontSendNotification);

    deviceBox.clear (juce::dontSendNotification);
    if (auto* type = manager.getCurrentDeviceTypeObject())
    {
        const auto names = type->getDeviceNames (false);
        for (int i = 0; i < names.size(); ++i)
            deviceBox.addItem (names[i], i + 1);
    }
    deviceBox.setText (manager.getAudioDeviceSetup().outputDeviceName, juce::dontSendNotification);

    rateBox.clear (juce::dontSendNotification);
    bufferBox.clear (juce::dontSendNotification);
    auto* device = manager.getCurrentAudioDevice();
    if (device != nullptr)
    {
        const auto rate = device->getCurrentSampleRate();
        for (auto r : device->getAvailableSampleRates())
            rateBox.addItem (juce::String ((int) r) + " Hz", (int) r);
        rateBox.setSelectedId ((int) rate, juce::dontSendNotification);

        for (auto size : device->getAvailableBufferSizes())
        {
            const auto ms = juce::String (rate > 0 ? size * 1000.0 / rate : 0.0, 1).replaceCharacter ('.', ',');
            bufferBox.addItem (juce::String (size) + " Samples, " + ms + " ms", size);
        }
        bufferBox.setSelectedId (device->getCurrentBufferSizeSamples(), juce::dontSendNotification);
    }
    rateBox.setEnabled (device != nullptr);
    bufferBox.setEnabled (device != nullptr);
    testButton.setEnabled (device != nullptr);

    status.setColour (juce::Label::textColourId, lastError.isNotEmpty() ? theme::downbeat : theme::textDim);
    status.setText (lastError.isNotEmpty() ? lastError
                    : device != nullptr ? "Spielt in Stereo auf Kanal 1 und 2."_u
                                        : "Kein Ausgang geöffnet."_u,
                    juce::dontSendNotification);
}

void AudioSettingsPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 6);
    auto row = [&r] (juce::Label& label, juce::Component& box, juce::Component* extra = nullptr)
    {
        auto line = r.removeFromTop (30);
        r.removeFromTop (8);
        label.setBounds (line.removeFromLeft (110));
        line.removeFromLeft (10);
        if (extra != nullptr)
        {
            extra->setBounds (line.removeFromRight (70));
            line.removeFromRight (8);
        }
        box.setBounds (line);
    };
    row (typeLabel, typeBox);
    row (deviceLabel, deviceBox, &testButton);
    row (rateLabel, rateBox);
    row (bufferLabel, bufferBox);
    status.setBounds (r.removeFromTop (24).withTrimmedLeft (120));
}
