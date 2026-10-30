#include "engine/TabParser.h"

#include "engine/MeterCatalog.h"

#include <cctype>
#include <map>
#include <sstream>

namespace hz
{

namespace
{

struct Lane
{
    Voice voice;
    LayerRole role;
    Limb limb;
};

const std::map<std::string, Lane> lanes {
    { "bd", { { Instrument::Kick,    Articulation::Normal }, LayerRole::Feet,  Limb::Auto } },
    { "br", { { Instrument::Kick,    Articulation::Normal }, LayerRole::Feet,  Limb::Right } },
    { "bl", { { Instrument::Kick,    Articulation::Normal }, LayerRole::Feet,  Limb::Left } },
    { "hp", { { Instrument::HiHat,   Articulation::Pedal },  LayerRole::Feet,  Limb::Left } },
    { "sn", { { Instrument::Snare,   Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "sr", { { Instrument::Snare,   Articulation::Rim },    LayerRole::Hands, Limb::Auto } },
    { "ss", { { Instrument::Snare,   Articulation::Side },   LayerRole::Hands, Limb::Auto } },
    { "bs", { { Instrument::Snare,   Articulation::Brush },  LayerRole::Hands, Limb::Auto } },
    { "ws", { { Instrument::Snare,   Articulation::Sweep },  LayerRole::Hands, Limb::Auto } },
    { "wl", { { Instrument::Snare,   Articulation::SweepLong }, LayerRole::Hands, Limb::Auto } },
    { "hh", { { Instrument::HiHat,   Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "rd", { { Instrument::Ride,    Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "rb", { { Instrument::Ride,    Articulation::Bell },   LayerRole::Hands, Limb::Auto } },
    { "rbr", { { Instrument::Ride,   Articulation::Brush },  LayerRole::Hands, Limb::Auto } },
    { "cr", { { Instrument::Crash1,  Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "c1", { { Instrument::Crash1,  Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "c2", { { Instrument::Crash2,  Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "ch", { { Instrument::China,   Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "sp", { { Instrument::Splash,  Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "t1", { { Instrument::Tom1,    Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "t2", { { Instrument::Tom2,    Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "tbr", { { Instrument::Tom2,   Articulation::Brush },  LayerRole::Hands, Limb::Auto } },
    { "t3", { { Instrument::Tom3,    Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "t4", { { Instrument::Tom4,    Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
    { "cb", { { Instrument::Cowbell, Articulation::Normal }, LayerRole::Hands, Limb::Auto } },
};

std::string trim (const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace ((unsigned char) s[a])) ++a;
    while (b > a && std::isspace ((unsigned char) s[b - 1])) --b;
    return s.substr (a, b - a);
}

std::vector<std::string> split (const std::string& s, char sep)
{
    std::vector<std::string> out;
    std::stringstream ss (s);
    std::string item;
    while (std::getline (ss, item, sep))
        out.push_back (trim (item));
    return out;
}

std::string slug (const std::string& s)
{
    std::string out;
    for (unsigned char c : s)
    {
        if (std::isalnum (c)) out += (char) std::tolower (c);
        else if (! out.empty() && out.back() != '-') out += '-';
    }
    while (! out.empty() && out.back() == '-') out.pop_back();
    return out;
}

bool parseFraction (const std::string& s, Rational& out)
{
    const auto slash = s.find ('/');
    try
    {
        if (slash == std::string::npos)
            return false;
        size_t usedN = 0, usedD = 0;
        const auto ns = s.substr (0, slash), ds = s.substr (slash + 1);
        const long long n = std::stoll (ns, &usedN);
        const long long d = std::stoll (ds, &usedD);
        if (usedN != ns.size() || usedD != ds.size())   // reject "1/16junk"
            return false;
        if (n <= 0 || d <= 0 || d > 256)
            return false;
        out = Rational (n, d);
        return true;
    }
    catch (...) { return false; }
}

struct Block
{
    std::map<std::string, std::string> header;
    std::vector<std::pair<int, std::string>> laneLines;   // line number, text
    int firstLine = 0;
};

std::string at (int line, const std::string& msg) { return "Zeile " + std::to_string (line) + ": " + msg; }

std::string buildPattern (const Block& b, Pattern& p)
{
    auto get = [&] (const char* k) { auto it = b.header.find (k); return it == b.header.end() ? std::string() : it->second; };

    p.name = get ("name");
    if (p.name.empty())
        return at (b.firstLine, "name fehlt");
    p.id = get ("id").empty() ? "lib." + slug (p.name) : get ("id");

    for (auto& t : split (get ("tags"), ','))
        if (! t.empty())
            p.tags.push_back (t);

    if (const auto tempo = get ("tempo"); ! tempo.empty())
    {
        double t = 0, lo = 0, hi = 0;
        if (std::sscanf (tempo.c_str(), "%lf (%lf-%lf)", &t, &lo, &hi) < 1 || t < 20 || t > 400)
            return at (b.firstLine, "tempo ungültig: " + tempo);
        p.tempo = t; p.tempoMin = lo; p.tempoMax = hi;
    }

    if (const auto h = get ("humanize"); ! h.empty())
        p.humanize = std::stof (h);

    if (const auto sw = get ("swing"); ! sw.empty())
    {
        const auto parts = split (sw, ' ');
        if (parts.size() < 2 || ! parseFraction (parts[0], p.swing.subdivision))
            return at (b.firstLine, "swing ungültig, Beispiel: swing: 1/8 0.62");
        p.swing.ratio = std::stof (parts.back());
    }

    // Collect tracks: track -> list of bar strings
    std::map<std::string, std::vector<std::pair<int, std::string>>> laneBars;
    std::map<std::string, int> laneDivision;   // own step count per bar (0 = grid)
    std::vector<std::string> laneOrder;
    for (const auto& [line, text] : b.laneLines)
    {
        const auto bar = text.find ('|');
        auto code = trim (text.substr (0, bar));

        // optional own subdivision of the bar: "cr(3)" = 3 equal steps per bar (polyrhythm)
        int division = 0;
        if (const auto paren = code.find ('('); paren != std::string::npos)
        {
            const auto close = code.find (')', paren);
            try { division = std::stoi (code.substr (paren + 1, close - paren - 1)); }
            catch (...) { division = -1; }
            if (close == std::string::npos || division < 1 || division > 128)
                return at (line, "Teilung ungültig, Beispiel: cr(3) | x x x |");
            code = trim (code.substr (0, paren));
            laneDivision[code] = division;
        }

        if (lanes.count (code) == 0)
            return at (line, "unbekannte Spur '" + code + "'");
        if (laneBars.count (code) == 0)
            laneOrder.push_back (code);

        // Bars between the bar lines; only the remainder after the last bar line may be empty
        auto segments = split (text.substr (bar + 1), '|');
        if (! segments.empty() && segments.back().empty())
            segments.pop_back();
        for (auto& seg : segments)
        {
            std::string steps;
            for (char c : seg)
                if (! std::isspace ((unsigned char) c))
                    steps += c;
            if (steps.empty())
                return at (line, "leerer Takt zwischen zwei Strichen");
            laneBars[code].push_back ({ line, steps });
        }
    }

    if (laneBars.empty())
        return at (b.firstLine, "keine Spuren");

    const size_t numBars = laneBars.begin()->second.size();
    for (auto& [code, bars] : laneBars)
        if (bars.size() != numBars)
            return at (bars.front().first, "Spur '" + code + "' hat " + std::to_string (bars.size()) + " Takte, andere " + std::to_string (numBars));

    // Meters and grid per bar (the last entry applies to the rest)
    auto meterTexts = split (get ("meter").empty() ? "4/4" : get ("meter"), ',');
    auto gridTexts = split (get ("grid").empty() ? "1/16" : get ("grid"), ',');

    for (size_t i = 0; i < numBars; ++i)
    {
        PatternBar bar;
        const auto& mt = meterTexts[std::min (i, meterTexts.size() - 1)];
        if (! parseMeter (mt, bar.meter))
            return at (b.firstLine, "meter ungültig: " + mt);
        const auto& gt = gridTexts[std::min (i, gridTexts.size() - 1)];
        if (! parseFraction (gt, bar.grid))
            return at (b.firstLine, "grid ungültig: " + gt);
        p.bars.push_back (bar);
    }

    PatternLayer hands { 1, LayerRole::Hands, {} };
    PatternLayer feet { 2, LayerRole::Feet, {} };
    std::uint32_t nextId = 1;

    for (const auto& code : laneOrder)
    {
        const auto& lane = lanes.at (code);
        const auto& bars = laneBars[code];

        for (size_t i = 0; i < numBars; ++i)
        {
            const auto& [line, steps] = bars[i];
            const auto& bar = p.bars[i];
            const int division = laneDivision.count (code) > 0 ? laneDivision[code] : 0;
            Rational stepsInBar (division);
            Rational stepLength = bar.grid * Rational (4);   // in quarters
            if (division > 0)
            {
                stepLength = bar.lengthInQuarters() / Rational (division);
            }
            else
            {
                stepsInBar = bar.lengthInQuarters() / stepLength;
                if (stepsInBar.den != 1)
                    return at (line, "Raster " + std::to_string (bar.grid.num) + "/" + std::to_string (bar.grid.den)
                                     + " passt nicht in die Taktart " + meterLabel (bar.meter));
            }
            if ((std::int64_t) steps.size() != stepsInBar.num)
                return at (line, "Spur '" + code + "' Takt " + std::to_string (i + 1) + ": " + std::to_string (steps.size())
                                 + " Zeichen, erwartet " + std::to_string (stepsInBar.num));

            const Rational start = p.barStart ((int) i);
            for (size_t s = 0; s < steps.size(); ++s)
            {
                const char c = steps[s];
                if (c == '.' || c == '-')
                    continue;

                PatternEvent e;
                e.id = nextId++;
                e.voice = lane.voice;
                e.limb = lane.limb;
                e.pos = start + Rational ((std::int64_t) s) * stepLength;

                switch (c)
                {
                    case 'g': e.velocity = 0.3f; break;
                    case 'x': e.velocity = 0.75f; break;
                    case 'X': e.velocity = 1.0f; break;
                    case 'f': e.velocity = 0.75f; e.flam = true; break;
                    case 'F': e.velocity = 1.0f; e.flam = true; break;
                    case 'o': case 'O': case 'h':
                        if (lane.voice.instrument != Instrument::HiHat || lane.voice.articulation != Articulation::Normal)
                            return at (line, std::string ("'") + c + "' gibt es nur in der hh-Spur");
                        e.voice.articulation = c == 'h' ? Articulation::Half : Articulation::Open;
                        e.velocity = c == 'O' ? 1.0f : 0.75f;
                        break;
                    default:
                        return at (line, std::string ("unbekanntes Zeichen '") + c + "'");
                }

                (lane.role == LayerRole::Feet ? feet : hands).events.push_back (e);
            }
        }
    }

    p.layers = { hands, feet };
    if (auto err = p.validate(); ! err.empty())
        return at (b.firstLine, err);
    return {};
}

} // namespace

TabParseResult parseTabs (const std::string& text)
{
    TabParseResult result;
    std::vector<Block> blocks (1);

    std::stringstream ss (text);
    std::string raw;
    int lineNo = 0;
    while (std::getline (ss, raw))
    {
        ++lineNo;
        auto line = trim (raw.substr (0, raw.find ('#')));
        if (line.empty())
            continue;

        if (line == "---")
        {
            blocks.emplace_back();
            continue;
        }

        auto& b = blocks.back();
        if (b.firstLine == 0)
            b.firstLine = lineNo;

        const auto colon = line.find (':');
        const auto bar = line.find ('|');
        if (bar != std::string::npos && (colon == std::string::npos || bar < colon))
            b.laneLines.push_back ({ lineNo, line });
        else if (colon != std::string::npos)
            b.header[trim (line.substr (0, colon))] = trim (line.substr (colon + 1));
        else
        {
            result.error = at (lineNo, "weder 'schlüssel: wert' noch Spur '.. | ... |'");
            return result;
        }
    }

    for (const auto& b : blocks)
    {
        if (b.firstLine == 0)
            continue;
        Pattern p;
        std::string err;
        try { err = buildPattern (b, p); }
        catch (const std::exception&) { err = at (b.firstLine, "Zahl nicht lesbar"); }
        if (! err.empty())
        {
            result.error = err;
            result.patterns.clear();
            return result;
        }
        result.patterns.push_back (std::move (p));
    }

    return result;
}

} // namespace hz
