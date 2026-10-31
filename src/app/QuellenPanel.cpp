#include "app/QuellenPanel.h"

#include "app/Text.h"
#include "app/views/Theme.h"

#include <memory>

namespace
{
    juce::File quellenFolder()   // where tools/import/quellen.py keeps what it loads, one folder per source
    {
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache/HZ-Drummer/quellen");
    }

    juce::File stagingFor (const juce::String& id)
    {
        return quellenFolder().getChildFile ("staging/" + id);
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

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&blocks, false);
    viewport.setScrollBarsShown (true, false);
    for (const auto& s : sources)
    {
        auto* tick = ticks.add (new juce::ToggleButton (s.name));
        blocks.addAndMakeVisible (tick);
        tick->setColour (juce::ToggleButton::textColourId, theme::textMain);
        tick->setColour (juce::ToggleButton::tickColourId, theme::textMain);
        tick->onClick = [this]
        {
            bool any = false;
            for (auto* t : ticks)
                any = any || t->getToggleState();
            importButton.setEnabled (any && ! isThreadRunning());
        };
        auto* folder = folders.add (new juce::TextButton ("Ordner"));
        blocks.addAndMakeVisible (folder);
        folder->onClick = [this, id = s.id] { openFolder (quellenFolder().getChildFile (id)); };
        auto* label = details.add (new juce::Label());
        blocks.addAndMakeVisible (label);
        label->setJustificationType (juce::Justification::topLeft);
        label->setColour (juce::Label::textColourId, theme::textDim);
        label->setFont (juce::FontOptions (13.5f));
        showSource (ticks.size() - 1);
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
    addAndMakeVisible (rootButton);
    rootButton.onClick = [this] { openFolder (quellenFolder()); };
    rootButton.setEnabled (quellenFolder().isDirectory());
    rootButton.setTooltip ("Der Ordner mit allen geladenen Quellen im Dateimanager");

    // as high as the sources need, but not higher than the screen: then the list scrolls
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const int most = display != nullptr ? display->userArea.getHeight() - 120 : 800;
    setSize (780, juce::jmin (most, 24 + 84 + (int) sources.size() * 116 + 16 + 30 + 12 + 40 + 20));
}

QuellenPanel::~QuellenPanel()
{
    signalThreadShouldExit();
    process.kill();
    stopThread (4000);
}

void QuellenPanel::showSource (int index)
{
    const auto& s = sources[(size_t) index];
    const juce::String dot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    juce::String text = s.text + "\nHerkunft: " + s.origin + dot + s.size + "\n" + s.hint;
    if (s.present)
        text << dot << "schon da";
    if (! s.usable)
        text << "\nGeht hier nicht: " << s.reason;
    details[index]->setText (text, juce::dontSendNotification);
    // there already: a ticked, greyed box, so it is not loaded twice; not usable here: greyed and empty
    ticks[index]->setToggleState (s.present, juce::dontSendNotification);
    ticks[index]->setEnabled (s.usable && ! s.present);   // during an import startImport() greys them all
    ticks[index]->setTooltip (s.present ? "Schon im Datenordner" : juce::String());
    const bool loaded = quellenFolder().getChildFile (s.id).isDirectory();
    folders[index]->setEnabled (loaded);
    folders[index]->setTooltip (loaded ? "Originaldateien und Umwandlung dieser Quelle im Dateimanager" : "Noch nichts geladen");
}

void QuellenPanel::openFolder (const juce::File& folder)
{
    if (! folder.isDirectory() || ! folder.startAsProcess())   // on Linux through xdg-open
        status.setText ("Ordner lässt sich nicht öffnen: "_u + folder.getFullPathName(), juce::dontSendNotification);
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
    auto buttons = r.removeFromBottom (40);   // status and buttons stay in place, the sources scroll above them
    r.removeFromBottom (12);
    status.setBounds (r.removeFromBottom (30));
    r.removeFromBottom (8);
    viewport.setBounds (r);
    const int height = ticks.size() * 116;
    blocks.setSize (r.getWidth() - (height > r.getHeight() ? viewport.getScrollBarThickness() + 6 : 0), height);
    for (int i = 0; i < ticks.size(); ++i)
    {
        juce::Rectangle<int> block (0, i * 116, blocks.getWidth(), 116);   // the name, then up to five lines of detail
        auto row = block.removeFromTop (26);
        folders[i]->setBounds (row.removeFromRight (84));
        ticks[i]->setBounds (row.withTrimmedRight (8));
        details[i]->setBounds (block.withTrimmedLeft (28).withTrimmedBottom (6));
    }
    importButton.setBounds (buttons.removeFromRight (160));
    buttons.removeFromRight (10);
    logButton.setBounds (buttons.removeFromRight (140));
    buttons.removeFromRight (10);
    rootButton.setBounds (buttons.removeFromRight (150));
}

void QuellenPanel::startImport()
{
    if (isThreadRunning())
        return;
    queue.clear();
    for (int i = 0; i < ticks.size(); ++i)
        if (ticks[i]->getToggleState() && sources[(size_t) i].usable && ! sources[(size_t) i].present)
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
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<QuellenPanel> (this), id]
        {
            if (safe == nullptr)
                return;
            for (size_t i = 0; i < safe->sources.size(); ++i)
                if (safe->sources[i].id == id)
                    safe->sources[i].present = true;   // done: ticked and greyed from now on
        });
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
        showSource (i);
    importButton.setEnabled (false);
    rootButton.setEnabled (quellenFolder().isDirectory());
}
