#include "app/SearchIndex.h"

#include "app/Library.h"
#include "app/Song.h"
#include "app/QuickSearch.h"
#include "app/Text.h"
#include "engine/MeterCatalog.h"

#include <algorithm>

//==============================================================================
juce::String SearchIndex::groupTitle (SearchItem::Group g)
{
    switch (g)
    {
        case SearchItem::Group::Songs:    return "Songs";
        case SearchItem::Group::Sections: return "Song-Abschnitte";
        case SearchItem::Group::Bundled:  return "Bibliothek";
        case SearchItem::Group::Own:      return "Eigene Patterns";
    }
    return {};
}

void SearchIndex::build (const Library& lib)
{
    items.clear();
    for (int i = 0; i < lib.size(); ++i)
    {
        const auto& p = lib.get (i);
        SearchItem it;
        it.id = p.id;
        it.title = juce::String (p.name);
        it.tempo = p.tempo;
        it.bars = (int) p.bars.size();
        it.favorite = lib.isFavorite (p.id);

        it.colour = lib.isSong (i) ? -1 : sourceColourIndex (p.id);
        it.shape = sourceShapeIndex (p.id);
        if (lib.isSong (i))
            it.group = SearchItem::Group::Songs;
        else if (lib.isUserPattern (i))
        {
            const auto group = Library::groupOf (p);
            it.group = group == "Einzelne" ? SearchItem::Group::Own : SearchItem::Group::Sections;
            it.origin = it.group == SearchItem::Group::Sections
                ? (Library::bandOf (p) == "Ohne Band" ? group : Library::bandOf (p) + " – " + group)
                : juce::String();
        }
        else
        {
            it.group = SearchItem::Group::Bundled;
            it.origin = p.tags.empty() ? juce::String() : Library::styleTitle (p.tags.front());
        }

        for (auto& b : p.bars)
        {
            const juce::String label (hz::meterLabel (b.meter));
            if (std::find (it.meters.begin(), it.meters.end(), label) == it.meters.end())
            {
                it.meters.push_back (label);
                it.meterNumbers.push_back ({ b.meter.numerator, b.meter.denominator });
            }
        }

        const auto sep = juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
        it.detail = Library::meterText (p) + sep + juce::String (it.bars) + (it.bars == 1 ? " Takt" : " Takte")
                  + sep + juce::String ((int) p.tempo) + " BPM";

        juce::StringArray words { it.title, it.origin, groupTitle (it.group), Library::meterText (p), juce::String ((int) p.tempo) };
        for (auto& t : p.tags)
            words.add (juce::String (t) + " " + Library::styleTitle (t));
        it.haystack = words.joinIntoString (" ").toLowerCase();
        items.push_back (std::move (it));
    }
}

int SearchIndex::indexOf (const std::string& id) const
{
    for (int i = 0; i < size(); ++i)
        if (items[(size_t) i].id == id)
            return i;
    return -1;
}

std::vector<SearchGroup> SearchIndex::search (const juce::String& text, int perGroup,
                                              const std::set<SearchItem::Group>& expanded) const
{
    const auto q = SearchQuery::parse (text);
    struct Hit { int score; int index; };
    std::vector<Hit> hits;

    for (int i = 0; i < size(); ++i)
    {
        const auto& it = items[(size_t) i];
        const int score = q.score (it.title, it.haystack, it.tempo, it.meterNumbers, it.favorite);
        if (score <= 0)
            continue;
        hits.push_back ({ score, i });
    }

    std::stable_sort (hits.begin(), hits.end(), [&] (const Hit& a, const Hit& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return items[(size_t) a.index].title.compareIgnoreCase (items[(size_t) b.index].title) < 0;
    });

    std::vector<SearchGroup> groups;
    for (auto g : { SearchItem::Group::Songs, SearchItem::Group::Sections, SearchItem::Group::Bundled, SearchItem::Group::Own })
    {
        SearchGroup sg { g, groupTitle (g), {}, 0 };
        const bool unlimited = perGroup <= 0 || expanded.count (g) > 0;
        for (auto& h : hits)
            if (items[(size_t) h.index].group == g)
            {
                ++sg.total;
                if (unlimited || (int) sg.items.size() < perGroup)
                    sg.items.push_back (h.index);
            }
        if (sg.total > 0)
            groups.push_back (std::move (sg));
    }
    return groups;
}
