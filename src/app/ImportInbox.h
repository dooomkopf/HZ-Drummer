#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <functional>
#include <map>
#include <memory>

// The import folder ~/HZ-Drummer-Import: a file put there ("Band - Titel.gp5", MuseScore, MIDI) is prepared in the
// background by tools/import/eingang.py, one at a time; the owner installs it (onResult, on the message thread, while
// the worker waits) and moves the file to "erledigt" or "Fehler". A band whose albums could not be fetched (no network)
// is noted and asked for again a minute after the start and every half hour. Nothing here shows a window.
class ImportInbox : private juce::Timer, private juce::Thread
{
public:
    struct Result
    {
        juce::File file;        // the file in the folder; empty for an album job
        juce::String band;      // album job: the band
        juce::File staging;     // what eingang.py prepared
        int exitCode = -1;      // 0 done, 2 done but no albums (network), 3 file not usable, 4 no band and title
                                // (it stays for renaming), 1 an error;
                                // -1: eingang.py did not run to its end (no "ERGEBNIS" line), the file is not at fault
        juce::String output;    // eingang.py's lines, for the import log
        juce::int64 size = 0, modified = 0;   // the file when its conversion began: changed since, it is read again
    };

    ImportInbox (const juce::File& script, const juce::File& dataFolder);
    ~ImportInbox() override;

    std::function<void (const Result&)> onResult;
    std::function<bool()> canDeliver;   // false while a menu or dialog is open: the result waits, a click stays right
    std::function<void (const juce::String&)> onStart;   // on the message thread: the file being read now

    void start();                              // the folder with HOWTOIMPORT.txt, then a look every few seconds
    void ignore (const juce::File& f)          { ignored.addIfNotAlreadyThere (f.getFullPathName()); }   // never again
    static juce::File folder();                // ~/HZ-Drummer-Import
    static juce::File pendingFile();           // bands with open albums, one per line
    static bool importable (const juce::File&);   // by its ending; hidden and partial files never
    static void notePending (const juce::String& band, bool open);   // into or out of pendingFile
    static juce::String howto();               // the text of HOWTOIMPORT.txt
    static juce::String howtoBefore();         // its first wording: replaced unless someone changed it

private:
    void timerCallback() override;
    void run() override;
    Result prepare (const juce::File& file, const juce::String& band);   // on the worker thread
    void deliver (std::shared_ptr<Result>, std::shared_ptr<juce::WaitableEvent>);   // on the message thread

    juce::File script, data;
    std::map<juce::String, std::pair<juce::int64, juce::int64>> seen;   // path -> size, modified (ms) at the last look
    juce::CriticalSection lock;
    juce::Array<juce::File> files;    // ready, waiting
    juce::StringArray bands;          // album jobs, waiting
    juce::StringArray busy;           // paths waiting or in work: not taken twice
    juce::StringArray ignored;        // files that could not be moved away: not taken again this session
    juce::StringArray reported;       // files of another kind, told once in the log
    int ticks = 0;
    juce::ChildProcess process;
    juce::CriticalSection processLock;   // the worker starts and reaps it, the destructor may stop it
    bool running = false;                // under processLock: a child that has not been reaped
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);   // for the calls back once this is gone
};
