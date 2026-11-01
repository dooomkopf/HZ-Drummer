#include "app/ImportInbox.h"

#include <algorithm>

namespace
{
    constexpr int lookMs = 3000;   // how often the folder is looked at; a file counts once it is unchanged over a look
    const juce::StringArray endings { ".gp3", ".gp4", ".gp5", ".gpx", ".gp", ".mscz", ".mscx", ".mid", ".midi" };

    juce::File cacheFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache/HZ-Drummer");
    }

    juce::String albumJob (const juce::String& band)   { return "album:" + band; }

    void writeText (const juce::File& f, const juce::String& text)   { f.replaceWithText (text, false, false, "\n"); }
}

ImportInbox::ImportInbox (const juce::File& scriptFile, const juce::File& dataFolder)
    : juce::Thread ("Eingangsordner"), script (scriptFile), data (dataFolder)
{
}

ImportInbox::~ImportInbox()
{
    *alive = false;
    stopTimer();
    signalThreadShouldExit();
    {
        const juce::ScopedLock sl (processLock);
        if (running)
            process.kill();   // a conversion that runs: its output ends, the worker sees the end; a reaped one is left alone
    }
    stopThread (4000);
}

juce::File ImportInbox::folder()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("HZ-Drummer-Import");
}

juce::File ImportInbox::pendingFile()
{
    return cacheFolder().getChildFile ("eingang-alben-offen.txt");
}

bool ImportInbox::importable (const juce::File& f)
{
    return f.existsAsFile() && ! f.getFileName().startsWithChar ('.') && endings.contains (f.getFileExtension().toLowerCase());
}

void ImportInbox::notePending (const juce::String& band, bool open)
{
    auto lines = juce::StringArray::fromLines (pendingFile().loadFileAsString());
    lines.trim();
    lines.removeEmptyStrings();
    lines.removeString (band, true);
    if (open)
        lines.add (band);
    pendingFile().getParentDirectory().createDirectory();
    writeText (pendingFile(), lines.joinIntoString ("\n") + (lines.isEmpty() ? "" : "\n"));
}

juce::String ImportInbox::howto()
{
    return juce::String::fromUTF8 (R"howto(HZ-Drummer: eigene Tabs einspielen

1. Datei in diesen Ordner kopieren. Band und Titel nimmt HZ-Drummer aus dem
   Namen "Band - Titel.endung", sonst aus der Datei selbst (Guitar Pro, MuseScore).
   Fehlt beides, bleibt die Datei liegen: dann hier umbenennen, z. B.
   Deep Purple - Highway Star.gp5
   Archive (.zip, .rar) vorher entpacken.
2. HZ-Drummer erkennt sie nach ein paar Sekunden, wandelt das Schlagzeug
   um und sortiert den Song unter Band › Album › Song ein.
   Danach liegt die Datei in "erledigt", eine nicht lesbare in "Fehler".

Formate: Guitar Pro .gp3 .gp4 .gp5 .gpx .gp, MuseScore .mscz .mscx, MIDI .mid
Gelesen wird nur das Schlagzeug, bei MIDI Kanal 10.
Braucht Python 3; .gp3 bis .gp5 zusätzlich Java und TuxGuitar 1.5.6.
Gibt es den Song schon, entsteht eine zweite Fassung mit -v2.
Das Album sucht HZ-Drummer bei MusicBrainz im Internet. Ohne Netz steht
der Song erst direkt unter der Band und bekommt sein Album später.
Was nicht geklappt hat und warum: ~/.cache/HZ-Drummer/import.log
Nur für den eigenen Gebrauch: Tabs gehören ihren Urhebern.
)howto");
}

juce::String ImportInbox::howtoBefore()
{
    return juce::String::fromUTF8 (R"howto(HZ-Drummer: eigene Tabs einspielen

1. Datei in diesen Ordner kopieren, Name: Band - Titel.endung
   Beispiel: Deep Purple - Highway Star.gp5
2. HZ-Drummer erkennt sie nach ein paar Sekunden, wandelt das Schlagzeug
   um und sortiert den Song unter Band › Album › Song ein.
   Danach liegt die Datei in "erledigt", eine nicht lesbare in "Fehler".

Formate: Guitar Pro .gp3 .gp4 .gp5 .gpx .gp, MuseScore .mscz .mscx, MIDI .mid
Gelesen wird nur das Schlagzeug, bei MIDI Kanal 10.
Braucht Python 3; .gp3 bis .gp5 zusätzlich Java und TuxGuitar 1.5.6.
Gibt es den Song schon, entsteht eine zweite Fassung mit -v2.
Das Album sucht HZ-Drummer bei MusicBrainz im Internet. Ohne Netz steht
der Song erst direkt unter der Band und bekommt sein Album später.
Was nicht geklappt hat und warum: ~/.cache/HZ-Drummer/import.log
Nur für den eigenen Gebrauch: Tabs gehören ihren Urhebern.
)howto");
}

void ImportInbox::start()
{
    cacheFolder().getChildFile ("eingang").deleteRecursively();   // what a crash left behind
    timerCallback();
    startTimer (lookMs);
}

void ImportInbox::timerCallback()
{
    const auto dir = folder();
    const auto howtoFile = dir.getChildFile ("HOWTOIMPORT.txt");
    if (howtoFile.existsAsFile() && howtoFile.loadFileAsString().replace ("\r\n", "\n") == howtoBefore())
        writeText (howtoFile, howto());   // the program's own first wording, unchanged: the new one in its place
    if (! howtoFile.existsAsFile())   // the first start, or someone cleared the folder
    {
        dir.getChildFile ("erledigt").createDirectory();
        dir.getChildFile ("Fehler").createDirectory();
        writeText (dir.getChildFile ("HOWTOIMPORT.txt"), howto());
    }
    std::map<juce::String, std::pair<juce::int64, juce::int64>> now;
    bool work = false;
    {
        const juce::ScopedLock sl (lock);
        auto found = dir.findChildFiles (juce::File::findFiles, false);
        std::sort (found.begin(), found.end(), [] (const juce::File& a, const juce::File& b)   // in the order of the names
                   { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        for (const auto& f : found)
        {
            const auto name = f.getFileName();
            if (! importable (f) && ! name.startsWithChar ('.') && name != "HOWTOIMPORT.txt" && ! reported.contains (name))
            {
                reported.add (name);   // once a session: it stays where it is
                juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache/HZ-Drummer/import.log")
                    .appendText (juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S") + " Eingang: " + name
                                 + juce::String::fromUTF8 (": Format nicht unterstützt, bleibt liegen (siehe HOWTOIMPORT.txt)\n"));
            }
            if (! importable (f) || ignored.contains (f.getFullPathName()))
                continue;
            const auto path = f.getFullPathName();
            const std::pair<juce::int64, juce::int64> state { f.getSize(), f.getLastModificationTime().toMilliseconds() };
            now[path] = state;
            const auto before = seen.find (path);
            if (state.first > 0 && before != seen.end() && before->second == state && ! busy.contains (path))
            {
                busy.add (path);   // copied to the end: unchanged since the last look
                files.add (f);
            }
        }
        if (++ticks == 60000 / lookMs || ticks % (1800000 / lookMs) == 0)   // a minute after the start, then every half hour
            for (auto band : juce::StringArray::fromLines (pendingFile().loadFileAsString()))
                if ((band = band.trim()).isNotEmpty() && ! busy.contains (albumJob (band)))
                {
                    busy.add (albumJob (band));
                    bands.add (band);
                }
        work = ! files.isEmpty() || ! bands.isEmpty();
    }
    seen = std::move (now);
    if (work && ! isThreadRunning())
        startThread();
}

void ImportInbox::run()
{
    while (! threadShouldExit())
    {
        juce::File file;
        juce::String band;
        {
            const juce::ScopedLock sl (lock);
            if (! files.isEmpty())
                file = files.removeAndReturn (0);
            else if (! bands.isEmpty())
            {
                band = bands[0];
                bands.remove (0);
            }
            else
                return;   // started again by the next look that finds something
        }
        juce::MessageManager::callAsync ([this, alive = alive, name = band.isNotEmpty() ? "Alben: " + band : file.getFileName()]
        {
            if (*alive && onStart)
                onStart (name);
        });
        auto result = std::make_shared<Result> (prepare (file, band));
        if (threadShouldExit())
            return;
        // installed on the message thread; the next file waits for it, it must see this one in the data folder
        auto handled = std::make_shared<juce::WaitableEvent>();
        juce::MessageManager::callAsync ([this, alive = alive, result, handled]
        {
            if (*alive)
                deliver (result, handled);
            else
                handled->signal();
        });
        while (! handled->wait (200))
            if (threadShouldExit())
                return;
    }
}

void ImportInbox::deliver (std::shared_ptr<Result> result, std::shared_ptr<juce::WaitableEvent> handled)
{
    if (canDeliver && ! canDeliver())   // a menu is open: its entries would shift under the mouse; try again soon
    {
        juce::Timer::callAfterDelay (500, [this, alive = alive, result, handled]
        {
            if (*alive)
                deliver (result, handled);
            else
                handled->signal();
        });
        return;
    }
    if (onResult)
        onResult (*result);
    result->staging.deleteRecursively();
    {
        const juce::ScopedLock sl (lock);
        busy.removeString (result->band.isNotEmpty() ? albumJob (result->band) : result->file.getFullPathName());
    }
    handled->signal();
}

ImportInbox::Result ImportInbox::prepare (const juce::File& file, const juce::String& band)
{
    Result r;
    r.file = file;
    r.band = band;
    r.size = file.getSize();
    r.modified = file.getLastModificationTime().toMilliseconds();
    r.staging = cacheFolder().getChildFile ("eingang").getChildFile (juce::String (juce::Time::currentTimeMillis()));
    r.staging.deleteRecursively();
    r.staging.createDirectory();
    juce::StringArray command { "python3", "-I", script.getFullPathName() };
    if (band.isNotEmpty())
        command.addArray (juce::StringArray { "--alben", band, r.staging.getFullPathName() });
    else
        command.addArray (juce::StringArray { file.getFullPathName(), r.staging.getFullPathName() });
    command.addArray (juce::StringArray { "--daten", data.getFullPathName() });
    {
        const juce::ScopedLock sl (processLock);
        if (threadShouldExit())
            return r;
        if (! process.start (command))
        {
            r.output = "python3 fehlt oder startet nicht";
            return r;
        }
        running = true;
    }
    juce::MemoryBlock out;
    char buffer[1024];
    for (int n = 0; ! threadShouldExit() && (n = process.readProcessOutput (buffer, sizeof (buffer))) > 0;)
        out.append (buffer, (size_t) n);
    {
        const juce::ScopedLock sl (processLock);
        if (threadShouldExit())
            process.kill();
        process.waitForProcessToFinish (10000);
        const auto code = (int) process.getExitCode();
        running = false;
        r.output = juce::String::fromUTF8 (static_cast<const char*> (out.getData()), (int) out.getSize()).trimEnd();
        // the result is eingang.py's last line: a child killed by a signal reads as 0, a missing script as 2
        const auto last = juce::StringArray::fromLines (r.output).strings.getLast();
        if (last.startsWith ("ERGEBNIS ") && last.substring (9).trim().containsOnly ("0123456789"))
            r.exitCode = last.substring (9).getIntValue();
        else
            r.output << juce::String::fromUTF8 ("\n(eingang.py ohne Ergebnis beendet, Rückgabe ") << code << ")";
    }
    return r;
}
