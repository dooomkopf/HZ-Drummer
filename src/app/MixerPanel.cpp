#include "app/MixerPanel.h"
#include "app/Text.h"

#include <cmath>

namespace
{
    constexpr float minDb = -48.0f, maxDb = 6.0f;

    const juce::Colour panelBg   { 0xff1c1c20 };
    const juce::Colour trackBg   { 0xff2a2a31 };
    const juce::Colour textDim   { 0xff8a8a92 };
    const juce::Colour fill      { 0xff48a0ff };
    const juce::Colour fillHot   { 0xffe89a2c };   // above 0 dB
    const juce::Colour muteOn    { 0xffe0443a };
    const juce::Colour soloOn    { 0xffe8c547 };
    const juce::Colour meterOk   { 0xff3fbf6a };
    const juce::Colour meterWarm { 0xffe8c547 };   // close to full scale
    const juce::Colour meterHot  { 0xffe0443a };

    constexpr int meterHz = 15;            // a calm display is enough to see where there is signal
    constexpr float meterFallDb = 1.5f;    // per tick: about 22 dB per second
    constexpr float paleAlpha = 0.35f;     // fader of a channel that does not sound
}

// Classic fader: narrow track, lit (coloured) below the thumb, scale ticks on both sides
class MixerPanel::FaderLook : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        const float cx = (float) x + (float) w * 0.5f;
        const float top = (float) y, bottom = (float) (y + h);
        const bool master = slider.getProperties()["master"];
        const auto zeroY = (float) slider.getPositionOfValue (0.0);

        const float tw = 6.0f;
        auto track = juce::Rectangle<float> (cx - tw * 0.5f, top, tw, bottom - top);
        g.setColour (trackBg);
        g.fillRoundedRectangle (track, 3.0f);
        g.setColour (sliderPos < zeroY ? fillHot : (master ? fillHot.withSaturation (0.4f) : fill));
        g.fillRoundedRectangle (track.withTop (sliderPos), 3.0f);

        // scale ticks
        g.setColour (textDim.withAlpha (0.5f));
        for (float yy = bottom; yy > top; yy -= 12.0f)
        {
            g.fillRect (cx - 18.0f, yy, 7.0f, 1.0f);
            g.fillRect (cx + 11.0f, yy, 7.0f, 1.0f);
        }

        // 0 dB mark
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRect (cx - 22.0f, zeroY, 6.0f, 1.5f);
        g.fillRect (cx + 16.0f, zeroY, 6.0f, 1.5f);

        // thumb (fader cap)
        auto thumb = juce::Rectangle<float> (42.0f, 18.0f).withCentre ({ cx, sliderPos });
        g.setColour (juce::Colour (0xffd8d8de));
        g.fillRoundedRectangle (thumb, 4.0f);
        g.setColour (juce::Colour (0xff3a3a42));
        g.fillRect (thumb.reduced (6.0f, 0).withHeight (2.0f).withCentre (thumb.getCentre()));
    }
};

MixerPanel::MixerPanel (const mix::Settings& initial, std::function<void (int, Setting)> callback,
                        std::function<float (int)> levelSource, std::function<bool (int)> usedSource)
    : look (std::make_unique<FaderLook>()), onChange (std::move (callback)),
      takeLevel (std::move (levelSource)), inUse (std::move (usedSource))
{
    for (int ch = 0; ch < mix::NumChannels; ++ch)
    {
        auto& s = strips[(size_t) ch];

        addAndMakeVisible (s.slider);
        s.slider.setLookAndFeel (look.get());
        s.slider.setSliderStyle (juce::Slider::LinearVertical);
        s.slider.setRange (minDb, maxDb, 0.5);
        s.slider.setSkewFactorFromMidPoint (-12.0);
        s.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 22);
        s.slider.setTextValueSuffix (" dB");
        s.slider.setValue (initial[(size_t) ch].db, juce::dontSendNotification);
        s.slider.setDoubleClickReturnValue (true, 0.0);
        s.slider.getProperties().set ("master", ch == mix::Master);
        s.slider.onValueChange = [this, ch] { notify (ch); };

        for (auto* b : { &s.mute, &s.solo })
        {
            addAndMakeVisible (*b);
            b->setClickingTogglesState (true);
            b->onClick = [this, ch] { notify (ch); };
        }
        s.mute.setToggleState (initial[(size_t) ch].muted, juce::dontSendNotification);
        s.mute.setColour (juce::TextButton::buttonOnColourId, muteOn);
        s.mute.setTooltip ("Stumm");
        s.solo.setToggleState (initial[(size_t) ch].solo, juce::dontSendNotification);
        s.solo.setColour (juce::TextButton::buttonOnColourId, soloOn);
        s.solo.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        s.solo.setTooltip ("Solo: nur diese Spur(en) hören");
        s.solo.setVisible (ch != mix::Master);   // master has no solo

        addAndMakeVisible (s.name);
        s.name.setText (juce::String (juce::CharPointer_UTF8 (mix::channelName (ch))), juce::dontSendNotification);
        s.name.setJustificationType (juce::Justification::centred);
        s.name.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    }

    addAndMakeVisible (resetButton);
    resetButton.onClick = [this]
    {
        for (int ch = 0; ch < mix::NumChannels; ++ch)
        {
            auto& s = strips[(size_t) ch];
            s.mute.setToggleState (false, juce::dontSendNotification);
            s.solo.setToggleState (false, juce::dontSendNotification);
            s.slider.setValue (0.0, juce::dontSendNotification);
            notify (ch);
        }
    };

    setSize (8 * 100 + 40, 520);
    startTimerHz (meterHz);
}

MixerPanel::~MixerPanel()
{
    stopTimer();
    for (auto& s : strips)
        s.slider.setLookAndFeel (nullptr);
}

void MixerPanel::notify (int ch)
{
    auto& s = strips[(size_t) ch];
    if (onChange)
        onChange (ch, { (float) s.slider.getValue(), s.mute.getToggleState(), s.solo.getToggleState() });
}

void MixerPanel::timerCallback()
{
    if (! isShowing())
        return;

    for (int ch = 0; ch < mix::NumChannels; ++ch)
    {
        auto& s = strips[(size_t) ch];

        const bool used = inUse == nullptr || inUse (ch);
        if (used != s.inUse)
        {
            s.inUse = used;
            s.slider.setAlpha (used ? 1.0f : paleAlpha);
            s.name.setAlpha (used ? 1.0f : paleAlpha);
        }

        const float level = takeLevel != nullptr ? takeLevel (ch) : 0.0f;
        const float db = level > 0.0f ? 20.0f * std::log10 (level) : minDb - 1.0f;
        const float next = juce::jmax (db, s.meterDb - meterFallDb);
        if (std::abs (next - s.meterDb) > 0.05f && (next >= minDb || s.meterDb >= minDb))
            repaint (meterArea (ch));
        s.meterDb = juce::jmax (next, minDb - 1.0f);
    }
}

juce::Rectangle<int> MixerPanel::meterArea (int ch) const
{
    // right of the fader's scale ticks, spanning the same dB range as the fader
    const auto& slider = strips[(size_t) ch].slider;
    const int x = slider.getX() + slider.getWidth() / 2 + 27;
    const int top = slider.getY() + (int) slider.getPositionOfValue (maxDb);
    const int bottom = slider.getY() + (int) slider.getPositionOfValue (minDb);
    return { x, top, 5, juce::jmax (0, bottom - top) };
}

void MixerPanel::paint (juce::Graphics& g)
{
    g.fillAll (panelBg);

    for (int ch = 0; ch < mix::NumChannels; ++ch)
    {
        const auto& s = strips[(size_t) ch];
        const auto area = meterArea (ch).toFloat();
        g.setColour (trackBg.withAlpha (s.inUse ? 1.0f : paleAlpha));
        g.fillRoundedRectangle (area, 2.0f);
        if (s.meterDb < minDb)
            continue;
        const auto y = (float) s.slider.getY() + (float) s.slider.getPositionOfValue (juce::jmin ((double) s.meterDb, (double) maxDb));
        g.setColour (s.meterDb > -1.0f ? meterHot : s.meterDb > -9.0f ? meterWarm : meterOk);
        g.fillRoundedRectangle (area.withTop (y), 2.0f);
    }

    // Visually set off the total
    const auto master = strips[mix::Master].name.getBounds().getUnion (strips[mix::Master].mute.getBounds()).expanded (8, 8);
    g.setColour (juce::Colour (0xff24242a));
    g.fillRoundedRectangle (master.toFloat(), 8.0f);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("Doppelklick auf einen Regler: 0 dB   ·   M = stumm   ·   S = solo   ·   blass = spielt hier nicht"_u,
                getLocalBounds().reduced (20, 14).removeFromBottom (32), juce::Justification::centredLeft);
}

void MixerPanel::resized()
{
    auto area = getLocalBounds().reduced (20, 16);

    auto bottom = area.removeFromBottom (32);
    resetButton.setBounds (bottom.removeFromRight (110));

    area.removeFromBottom (18);   // gap to the footer bar

    const int w = area.getWidth() / mix::NumChannels;
    for (int ch = 0; ch < mix::NumChannels; ++ch)
    {
        auto col = area.removeFromLeft (w).reduced (8, 0);
        if (ch == mix::Master)
            col = col.withTrimmedLeft (8);
        auto& s = strips[(size_t) ch];
        s.name.setBounds (col.removeFromTop (28));
        col.removeFromTop (8);

        auto buttons = col.removeFromBottom (30);
        if (ch == mix::Master)
            s.mute.setBounds (buttons.withSizeKeepingCentre (40, 30));
        else
        {
            s.mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2, 0));
            s.solo.setBounds (buttons.reduced (2, 0));
        }
        col.removeFromBottom (10);
        s.slider.setBounds (col);
    }
}
