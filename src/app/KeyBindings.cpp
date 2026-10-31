#include "app/KeyBindings.h"
#include "app/Text.h"

namespace
{
    using KP = juce::KeyPress;

    auto code (int c, bool shift)
    {
        return [c, shift] (const KP& k) { return k.getKeyCode() == c && k.getModifiers().isShiftDown() == shift && ! k.getModifiers().isCommandDown(); };
    }

    auto commandCode (int c)
    {
        return [c] (const KP& k) { return k.getKeyCode() == c && k.getModifiers().isCommandDown(); };
    }

    auto character (juce::juce_wchar c)
    {
        return [c] (const KP& k) { return ! k.getModifiers().isCommandDown() && juce::CharacterFunctions::toLowerCase (k.getTextCharacter()) == c; };
    }

    auto commandLetterOr (juce::juce_wchar letter, juce::juce_wchar alternative)
    {
        return [letter, alternative] (const KP& k)
        {
            return (k.getModifiers().isCommandDown() && juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) k.getKeyCode()) == letter)
                || k.getTextCharacter() == alternative;
        };
    }
}

std::vector<KeyBinding> makeKeyBindings (const KeyActions& a)
{
    return {
        { "Leertaste",            "Start / Pause (geht an derselben Stelle weiter)",  code (KP::spaceKey, false), a.togglePlay },
        { "A oder Pos1",          "Zurück an den Anfang",                            character ('a'),            a.rewind },
        { "",                     "",                                                code (KP::homeKey, false),  a.rewind },
        { "Pfeil hoch/runter",    "Tempo +1 / -1",                                   code (KP::upKey, false),    [a] { a.changeTempo (1.0); } },
        { "",                     "",                                                code (KP::downKey, false),  [a] { a.changeTempo (-1.0); } },
        { "Shift + hoch/runter",  "Tempo +5 / -5",                                   code (KP::upKey, true),     [a] { a.changeTempo (5.0); } },
        { "",                     "",                                                code (KP::downKey, true),   [a] { a.changeTempo (-5.0); } },
        { "Pfeil links/rechts",   "Weiter/zurück im Ordner, wie im Baustein-Menü; im Metronom die Taktart. Am Ende ist Schluss"_u,
                                                                                     code (KP::leftKey, false),  [a] { a.stepCurrent (-1, false); } },
        { "",                     "",                                                code (KP::rightKey, false), [a] { a.stepCurrent (1, false); } },
        { "Shift + links/rechts", "durch die Favoriten (Bausteine bzw. Taktarten)",  code (KP::leftKey, true),   [a] { a.stepCurrent (-1, true); } },
        { "",                     "",                                                code (KP::rightKey, true),  [a] { a.stepCurrent (1, true); } },
        { "Strg + links/rechts",  "Ordner wechseln, wie im Baustein-Menü"_u,          commandCode (KP::leftKey),  [a] { a.stepStyle (-1); } },
        { "",                     "",                                                commandCode (KP::rightKey), [a] { a.stepStyle (1); } },
        { "N / B",                "Arrangement: nächster / voriger Teil (oder in die Ablaufleiste klicken)"_u,
                                                                                     character ('n'),            [a] { a.seekPart (1); } },
        { "",                     "",                                                character ('b'),            [a] { a.seekPart (-1); } },
        { "P",                    "Baustein-Menü öffnen"_u,                           character ('p'),            a.patternMenu },
        { "M",                    "Mixer an/aus, auch in der Werkstatt"_u,             character ('m'),            a.mixer },
        { "",                     "",                                                character ('l'),            a.mixer },   // the former key
        { "E",                    "Werkstatt: Editor (Baustein) bzw. Arranger (Arrangement); in der Werkstatt schließt E sie wieder, wenn im Editor nichts ungespeichert ist"_u,
                                                                                     character ('e'),            a.editor },
        { "Strg+F oder *",        "Favorit an/aus (Baustein bzw. Taktart)",           commandLetterOr ('F', '*'), a.toggleFavorite },
        { "T",                    "Tap-Tempo (mehrmals im Takt tippen)",             character ('t'),            a.tapTempo },
        { "K",                    "Metronom-Klang (Klick, Bassdrum, Snare, Bass+Snare)", character ('k'),        a.nextSound },
        { "V",                    "Schläge unter den Zählzeiten anzeigen an/aus"_u,  character ('v'),            a.toggleBeatDisplay },
        { "Strg+K oder /",        "Schnellsuche: Bausteine, Arrangements, Taktarten (z.B. \"thr\" findet Thrash)",
                                                                                     commandLetterOr ('K', '/'), a.search },
        { "F1 oder ?",            "Diese Hilfe",
          [] (const KP& k) { return k.getKeyCode() == KP::F1Key || k.getTextCharacter() == '?'; },             a.help },
    };
}

bool isTransportKey (const juce::KeyPress& key)
{
    const int c = key.getKeyCode();
    const auto ch = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    return ! key.getModifiers().isCommandDown()
        && (c == KP::spaceKey || c == KP::upKey || c == KP::downKey || ch == 't' || ch == 'm');
}
