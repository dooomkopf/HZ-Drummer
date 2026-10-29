#include "app/SearchQuery.h"
#include "app/QuickSearch.h"

#include <algorithm>

namespace
{
    // "180", "160-190", ">180", "<120" -> range; false if it is not a tempo spec
    bool parseTempoSpec (juce::String spec, int& lo, int& hi)
    {
        spec = spec.replace (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")), "-").trim();   // en dash
        auto number = [] (const juce::String& s, int& out)
        {
            if (s.isEmpty() || ! s.containsOnly ("0123456789"))
                return false;
            out = s.getIntValue();
            return out >= 20 && out <= 400;
        };
        int a = 0, b = 0;
        if (spec.startsWithChar ('>'))  { if (! number (spec.substring (1), a)) return false; lo = a; hi = 400; return true; }
        if (spec.startsWithChar ('<'))  { if (! number (spec.substring (1), a)) return false; lo = 20; hi = a; return true; }
        if (spec.containsChar ('-'))
        {
            if (! number (spec.upToFirstOccurrenceOf ("-", false, false), a) || ! number (spec.fromFirstOccurrenceOf ("-", false, false), b))
                return false;
            lo = std::min (a, b);
            hi = std::max (a, b);
            return true;
        }
        if (! number (spec, a))
            return false;
        lo = a - 8;   // a single value means "around"
        hi = a + 8;
        return true;
    }

    bool isMeterToken (const juce::String& t)
    {
        const auto num = t.upToFirstOccurrenceOf ("/", false, false), den = t.fromFirstOccurrenceOf ("/", false, false);
        return t.containsChar ('/') && num.isNotEmpty() && den.isNotEmpty()
            && num.containsOnly ("0123456789") && den.containsOnly ("0123456789");
    }
}

SearchQuery SearchQuery::parse (const juce::String& text)
{
    SearchQuery q;
    juce::StringArray tokens;
    tokens.addTokens (text.toLowerCase(), " ", "");
    tokens.removeEmptyStrings();

    juce::StringArray words;
    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto& t = tokens[i];
        int lo = 0, hi = 0;
        if (isMeterToken (t))
            q.meter = t;
        else if (t.endsWith ("bpm") && t.length() > 3 && parseTempoSpec (t.dropLastCharacters (3), lo, hi))
            { q.tempoMin = lo; q.tempoMax = hi; }
        else if (i + 1 < tokens.size() && tokens[i + 1] == "bpm" && parseTempoSpec (t, lo, hi))
            { q.tempoMin = lo; q.tempoMax = hi; ++i; }
        else
            words.add (t);
    }
    q.words = words.joinIntoString (" ");
    if (! words.isEmpty() && ! tokens.isEmpty() && words[words.size() - 1] == tokens[tokens.size() - 1]
        && text.isNotEmpty() && juce::CharacterFunctions::isWhitespace (text.getLastCharacter()))
        q.words += " "; // a completed word is different from a prefix
    return q;
}

juce::String SearchQuery::tempoChip() const
{
    if (tempoMin <= 0)
        return {};
    if (tempoMax >= 400) return "Tempo > " + juce::String (tempoMin);
    if (tempoMin <= 20)  return "Tempo < " + juce::String (tempoMax);
    return "Tempo " + juce::String (tempoMin) + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + juce::String (tempoMax);
}

int SearchQuery::score (const juce::String& title, const juce::String& haystack, double tempo,
                        const std::vector<std::pair<int, int>>& meters, bool favorite) const
{
    if (meter.isNotEmpty())
    {
        const int num = meter.upToFirstOccurrenceOf ("/", false, false).getIntValue();
        const int den = meter.fromFirstOccurrenceOf ("/", false, false).getIntValue();
        if (std::none_of (meters.begin(), meters.end(),
                          [&] (const auto& value) { return value.first == num && value.second == den; }))
            return 0;
    }
    if (tempoMin > 0 && (tempo < tempoMin || tempo > tempoMax))
        return 0;
    auto tokens = juce::StringArray::fromTokens (words, " ", "");
    tokens.removeEmptyStrings();
    int value = tokens.isEmpty() ? 1 : 0;
    const bool completed = words.endsWithChar (' ');
    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto token = tokens[i] + (completed && i == tokens.size() - 1 ? " " : "");
        const int match = juce::jmax (fuzzyScore (token, haystack, false), fuzzyScore (token, title, true));
        if (match == 0) return 0;
        value += match;
    }
    if (words.isNotEmpty() && title.toLowerCase().contains (words.trimEnd()))
        value += 300;
    return value + (favorite ? 150 : 0);
}
