#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// The small dot at the bottom right of the main window for the import folder: it pulses green while a file is read,
// shines green when there is news, orange when one of the new messages is a problem, and is gone otherwise. A click
// shows the messages, newest first; once that closes they count as read and the dot goes. The messages survive a
// restart (~/.cache/HZ-Drummer/eingang-meldungen.json, the last 50); the whole story stays in import.log.
class ImportNoticeDot : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    struct Notice
    {
        juce::String time, file, text;
        bool problem = false, read = false;
    };

    ImportNoticeDot();

    void add (const juce::String& file, const juce::String& text, bool problem);
    void setBusy (const juce::String& file);   // the file being read, "" when none
    std::function<void()> onOpenFolder, onShowLog;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void update();      // shown or not, colour, tooltip, pulse
    void markRead();
    void save() const;
    static juce::File file();

    std::vector<Notice> notices;   // oldest first
    juce::String busyFile;
    float phase = 0.0f;
};
