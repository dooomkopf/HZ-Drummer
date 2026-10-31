#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// System › Quellen: building blocks HZ-Drummer does not ship, because their licence does not allow it or does
// not say. A tick and "Importieren" load them from their original source and convert them on this computer, for
// one's own use (tools/import/quellen.py, one child process per source, its progress lines shown). Each finished
// source is put into the data folder through onInstall; what was dropped or simplified is in the import log.
class QuellenPanel : public juce::Component, private juce::Thread
{
public:
    std::function<juce::String (const juce::File& staging)> onInstall;   // message thread: put it in, say what came
    std::function<void()> onShowLog;                                     // the import log in System › Log-Dateien

    QuellenPanel (const juce::File& script, const juce::File& dataFolder);
    ~QuellenPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Source
    {
        juce::String id, name, text, origin, hint, size, reason;
        bool usable = true, present = false;
    };

    void run() override;   // the child processes, one source after the other
    void startImport();
    void setStatus (const juce::String&);   // any thread
    void finished();                         // message thread

    juce::File script, data;
    std::vector<Source> sources;
    juce::OwnedArray<juce::ToggleButton> ticks;
    juce::OwnedArray<juce::Label> details;
    juce::Label intro, status;
    juce::TextButton importButton { "Importieren" }, logButton { "Import-Log" };
    juce::StringArray queue;   // ids to load, set before the thread starts
    juce::ChildProcess process;
};
