#include "app/QuellenPanel.h"

#include "app/Text.h"
#include "app/views/Theme.h"

#include <memory>

namespace
{
    juce::File stagingFor (const juce::String& id)
    {
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache/HZ-Drummer/quellen/staging/" + id);
    }
}

QuellenPanel::QuellenPanel (const juce::File& scriptFile, const juce::File& dataFolder)
    : juce::Thread ("Quellen-Import"), script (scriptFile), data (dataFolder)
{
    addAndMakeVisible (intro);
    intro.setJustificationType (juce::Justification::topLeft);
    intro.setColour (juce::Label::textColourId, theme::textMain);
    intro.setFont (juce::FontOptions (15.0f));
    intro.setText ("Diese Bausteine liefert HZ-Drummer nicht mit, weil ihre Lizenz das Weitergeben nicht erlaubt oder "
                   "nicht regelt. Mit einem Haken und \"Importieren\" lädt das Programm sie von der Originalquelle und "
                   "wandelt sie auf diesem Rechner um, nur für den eigenen Gebrauch. Vorhandenes wird nie überschrieben; "
                   "was wegfällt oder vereinfacht wird, steht im Import-Log."_u, juce::dontSendNotification);

    // the list comes from the script itself: names, origins, hints, whether a source works here, what is there
    juce::ChildProcess list;
    juce::String json;
    if (script.existsAsFile() && list.start (juce::StringArray { "python3", "-I", script.getFullPathName(), "liste", data.getFullPathName() }))
    {
        json = list.readAllProcessOutput();
        list.waitForProcessToFinish (5000);
    }
    const auto parsed = juce::JSON::parse (json);   // kept: the array below lives inside it
    if (const auto* items = parsed.getArray())
        for (const auto& v : *items)
            sources.push_back ({ v["id"].toString(), v["name"].toString(), v["beschreibung"].toString(), v["herkunft"].toString(),
                                 v["hinweis"].toString(), v["groesse"].toString(), v["grund"].toString(),
                                 (bool) v["nutzbar"], (bool) v["vorhanden"] });

    for (const auto& s : sources)
    {
        auto* tick = ticks.add (new juce::ToggleButton (s.name));
        addAndMakeVisible (tick);
        tick->setColour (juce::ToggleButton::textColourId, theme::textMain);
        tick->setColour (juce::ToggleButton::tickColourId, theme::textMain);
        tick->setEnabled (s.usable);
        tick->onClick = [this]
        {
            bool any = false;
            for (auto* t : ticks)
                any = any || t->getToggleState();
            importButton.setEnabled (any && ! isThreadRunning());
        };
        auto* label = details.add (new juce::Label());
        addAndMakeVisible (label);
        label->setJustificationType (juce::Justification::topLeft);
        label->setColour (juce::Label::textColourId, theme::textDim);
        label->setFont (juce::FontOptions (13.5f));
        juce::String text = s.text + "\nHerkunft: " + s.origin + "  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  " + s.size
                          + "\n" + s.hint;
        if (s.present)
            text << "  " << juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) << "  schon da";
        if (! s.usable)
            text << "\nGeht hier nicht: " << s.reason;
        label->setText (text, juce::dontSendNotification);
    }

    addAndMakeVisible (status);
    status.setColour (juce::Label::textColourId, juce::Colour (0xffe07a70));   // reddish, as the help lines
    status.setFont (juce::FontOptions (15.0f));
    status.setText (sources.empty() ? "Die Quellen-Liste fehlt: Ist python3 installiert und tools/import/quellen.py da?"_u
                                    : juce::String(), juce::dontSendNotification);
    addAndMakeVisible (importButton);
    importButton.setEnabled (false);
    importButton.onClick = [this] { startImport(); };
    addAndMakeVisible (logButton);
    logButton.onClick = [this] { if (onShowLog) onShowLog(); };

    setSize (780, 24 + 84 + (int) sources.size() * 116 + 16 + 30 + 12 + 40 + 20);
}

QuellenPanel::~QuellenPanel()
{
    signalThreadShouldExit();
    process.kill();
    stopThread (4000);
}

void QuellenPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
}

void QuellenPanel::resized()
{
    auto r = getLocalBounds().reduced (20, 16);
    intro.setBounds (r.removeFromTop (84));
    r.removeFromTop (8);
    for (int i = 0; i < ticks.size(); ++i)
    {
        auto block = r.removeFromTop (116);   // the name, then up to five lines of detail
        ticks[i]->setBounds (block.removeFromTop (26));
        details[i]->setBounds (block.withTrimmedLeft (28).withTrimmedBottom (6));
    }
    r.removeFromTop (8);
    status.setBounds (r.removeFromTop (30));
    r.removeFromTop (12);
    auto buttons = r.removeFromTop (40);
    importButton.setBounds (buttons.removeFromRight (160));
    buttons.removeFromRight (10);
    logButton.setBounds (buttons.removeFromRight (140));
}

void QuellenPanel::startImport()
{
    if (isThreadRunning())
        return;
    queue.clear();
    for (int i = 0; i < ticks.size(); ++i)
        if (ticks[i]->getToggleState() && sources[(size_t) i].usable)
            queue.add (sources[(size_t) i].id);
    if (queue.isEmpty())
        return;
    importButton.setEnabled (false);
    for (auto* t : ticks)
        t->setEnabled (false);
    setStatus ("Startet ...");
    startThread();
}

void QuellenPanel::setStatus (const juce::String& text)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<QuellenPanel> (this), text]
    {
        if (safe != nullptr)
            safe->status.setText (text, juce::dontSendNotification);
    });
}

void QuellenPanel::run()
{
    juce::StringArray reports;
    for (const auto& id : queue)
    {
        if (threadShouldExit())
            return;
        juce::String name = id;
        for (const auto& s : sources)
            if (s.id == id)
                name = s.name;
        const auto staging = stagingFor (id);
        juce::Logger::writeToLog ("Quellen: " + name + " wird geladen");
        if (! process.start (juce::StringArray { "python3", "-I", script.getFullPathName(), "laden", id, staging.getFullPathName() }))
        {
            reports.add (name + ": python3 nicht gefunden");
            break;
        }
        // line by line (the pipe is buffered, so byte by byte): "SCHRITT ..." shows, "FERTIG"/"FEHLER" ends it
        juce::String line, done, error;
        char c = 0;
        while (! threadShouldExit() && process.readProcessOutput (&c, 1) == 1)
        {
            if (c != '\n')
            {
                line += juce::String::charToString ((juce::juce_wchar) (juce::uint8) c);
                continue;
            }
            const auto text = juce::String::fromUTF8 (line.toRawUTF8()).trim();
            line.clear();
            if (text.startsWith ("SCHRITT "))
                setStatus (text.substring (8));
            else if (text.startsWith ("FERTIG "))
                done = text.substring (7);
            else if (text.startsWith ("FEHLER "))
                error = text.substring (7);
        }
        if (threadShouldExit())
        {
            process.kill();
            return;
        }
        process.waitForProcessToFinish (10000);
        if (error.isNotEmpty() || done.isEmpty())
        {
            reports.add (name + ": " + (error.isNotEmpty() ? error : juce::String ("abgebrochen")));
            juce::Logger::writeToLog ("Quellen: " + reports[reports.size() - 1] + " (Exit-Code "
                                      + juce::String ((int) process.getExitCode()) + ")");
            continue;
        }

        // into the data folder on the message thread; the next source starts once that is done
        auto installed = std::make_shared<juce::WaitableEvent>();
        auto report = std::make_shared<juce::String>();
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<QuellenPanel> (this), staging, installed, report]
        {
            if (safe != nullptr && safe->onInstall)
                *report = safe->onInstall (staging);
            installed->signal();
        });
        installed->wait (120000);
        reports.add (name + ": " + *report);
        juce::Logger::writeToLog ("Quellen: " + reports[reports.size() - 1]);
    }
    setStatus (reports.joinIntoString ("  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  "));
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<QuellenPanel> (this)]
    {
        if (safe != nullptr)
            safe->finished();
    });
}

void QuellenPanel::finished()
{
    for (int i = 0; i < ticks.size(); ++i)
    {
        ticks[i]->setEnabled (sources[(size_t) i].usable);
        ticks[i]->setToggleState (false, juce::dontSendNotification);
    }
    importButton.setEnabled (false);
}
