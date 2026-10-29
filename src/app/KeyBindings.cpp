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
        { "Pfeil links/rechts",   "Weiter/zurück: Pattern (im Pattern) bzw. Taktart (im Metronom)"_u,
                                                                                     code (KP::leftKey, false),  [a] { a.stepCurrent (-1, false); } },
        { "",                     "",                                                code (KP::rightKey, false), [a] { a.stepCurrent (1, false); } },
        { "Shift + links/rechts", "dasselbe, nur Favoriten",                         code (KP::leftKey, true),   [a] { a.stepCurrent (-1, true); } },
        { "",                     "",                                                code (KP::rightKey, true),  [a] { a.stepCurrent (1, true); } },
        { "Strg + links/rechts",  "Pattern-Stil wechseln (Thrash, Death, Doom ...)", commandCode (KP::leftKey),  [a] { a.stepStyle (-1); } },
        { "",                     "",                                                commandCode (KP::rightKey), [a] { a.stepStyle (1); } },
        { "Bild auf/ab",          "Pattern: voriges / nächstes"_u,                   code (KP::pageUpKey, false),   [a] { a.stepPattern (-1, false); } },
        { "",                     "",                                                code (KP::pageDownKey, false), [a] { a.stepPattern (1, false); } },
        { "Shift + Bild auf/ab",  "Pattern: nur Favoriten",                          code (KP::pageUpKey, true),    [a] { a.stepPattern (-1, true); } },
        { "",                     "",                                                code (KP::pageDownKey, true),  [a] { a.stepPattern (1, true); } },
        { "N / B",                "Song: nächster / voriger Teil (oder in die Ablaufleiste klicken)"_u,
                                                                                     character ('n'),            [a] { a.seekPart (1); } },
        { "",                     "",                                                character ('b'),            [a] { a.seekPart (-1); } },
        { "P",                    "Pattern-Menü öffnen"_u,                           character ('p'),            a.patternMenu },
        { "M",                    "Taktart-Menü öffnen"_u,                           character ('m'),            a.meterMenu },
        { "E",                    "Werkstatt: Raster-Editor (Pattern) bzw. Arranger (Song)"_u, character ('e'),  a.editor },
        { "Strg+F oder *",        "Favorit an/aus (Pattern bzw. Taktart)",           commandLetterOr ('F', '*'), a.toggleFavorite },
        { "T",                    "Tap-Tempo (mehrmals im Takt tippen)",             character ('t'),            a.tapTempo },
        { "K",                    "Metronom-Klang (Klick, Bassdrum, Snare, Kick+Snare)", character ('k'),        a.nextSound },
        { "L",                    "Mixer: Lautstärke der Spuren"_u,                  character ('l'),            a.mixer },
        { "V",                    "Schläge unter den Zählzeiten anzeigen an/aus"_u,  character ('v'),            a.toggleBeatDisplay },
        { "Strg+K oder /",        "Schnellsuche: Patterns, Songs, Taktarten (z.B. \"thr\" findet Thrash)",
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
        && (c == KP::spaceKey || c == KP::upKey || c == KP::downKey || ch == 't');
}
