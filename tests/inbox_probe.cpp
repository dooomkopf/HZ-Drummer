// "eingang <datei>": the import folder end to end without the app window. Run it with HOME set to a sandbox (a copy
// of the data folder in $HOME/.config/HZ-Drummer): ImportInbox writes HOWTOIMPORT.txt, finds the copied file once it
// stands still, tools/import/eingang.py prepares it, the result comes back on the message thread.

#include "app/ImportInbox.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

int runInboxProbe (const juce::File& source, const juce::File& script)
{
    const juce::ScopedJuceInitialiser_GUI gui;
    const auto data = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("HZ-Drummer");
    ImportInbox inbox (script, data);
    int code = -99;
    const auto started = juce::Time::getMillisecondCounter();
    inbox.onResult = [&] (const ImportInbox::Result& r)
    {
        std::printf ("Ergebnis nach %.1f s: %s, Rückgabe %d, %d Bausteine, %d Songs, alben.json %s\n%s\n",
                     (juce::Time::getMillisecondCounter() - started) / 1000.0, r.file.getFileName().toRawUTF8(), r.exitCode,
                     r.staging.getChildFile ("patterns").getNumberOfChildFiles (juce::File::findFiles),
                     r.staging.getChildFile ("songs").getNumberOfChildFiles (juce::File::findFiles),
                     r.staging.getChildFile ("alben.json").existsAsFile() ? "ja" : "nein", r.output.toRawUTF8());
        code = r.exitCode;
        juce::MessageManager::getInstance()->stopDispatchLoop();
    };
    const auto target = ImportInbox::folder().getChildFile (source.getFileName());
    if (target.exists())   // never over a file that lies there already (HOME not set to a sandbox?)
    {
        std::printf ("Im Eingangsordner liegt schon %s: nichts getan (HOME auf eine Sandbox setzen)\n", target.getFullPathName().toRawUTF8());
        return -98;
    }
    inbox.start();
    std::printf ("Daten %s, HOWTOIMPORT.txt %s, kopiert: %s\n", data.getFullPathName().toRawUTF8(),
                 ImportInbox::folder().getChildFile ("HOWTOIMPORT.txt").existsAsFile() ? "da" : "fehlt",
                 source.copyFileTo (target) ? "ja" : "nein");
    juce::Timer::callAfterDelay (600000, [] { juce::MessageManager::getInstance()->stopDispatchLoop(); });   // at most 10 min
    juce::MessageManager::getInstance()->runDispatchLoop();
    target.deleteFile();   // the copy made above; the app would have moved it to "erledigt"
    return code;
}
