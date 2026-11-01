#include "app/AlbumView.h"

#include <algorithm>
#include <map>
#include <optional>
#include <tuple>

namespace
{
    // the tree while it is being built: folders by a sort key, so that albums come by year and songs by number
    struct Node
    {
        juce::String name;
        std::map<std::tuple<int, juce::String>, Node> folders;   // (rank, name) -> folder; rank sorts before the name
        std::vector<std::tuple<int, juce::String, std::string>> items;   // (rank, name, id)

        // key: what makes two folders one; by default the name without case
        Node& child (int rank, const juce::String& folderName, const juce::String& key = {})
        {
            auto& n = folders[{ rank, key.isNotEmpty() ? key : folderName.toLowerCase() }];
            if (n.name.isEmpty())
                n.name = folderName;
            return n;
        }
    };

    bool natural (const juce::String& a, const juce::String& b)   { return a.compareNatural (b, true) < 0; }

    FolderNode emit (Node& node)
    {
        FolderNode out;
        out.name = node.name;
        std::vector<std::pair<std::tuple<int, juce::String>, Node*>> ordered;
        for (auto& [k, n] : node.folders)
            ordered.push_back ({ k, &n });
        std::stable_sort (ordered.begin(), ordered.end(), [] (const auto& a, const auto& b)
        {
            if (std::get<0> (a.first) != std::get<0> (b.first))
                return std::get<0> (a.first) < std::get<0> (b.first);
            return natural (std::get<1> (a.first), std::get<1> (b.first));
        });
        for (auto& [k, n] : ordered)
            out.folders.push_back (emit (*n));
        std::stable_sort (node.items.begin(), node.items.end(), [] (const auto& a, const auto& b)
        {
            if (std::get<0> (a) != std::get<0> (b))
                return std::get<0> (a) < std::get<0> (b);
            return natural (std::get<1> (a), std::get<1> (b));
        });
        for (const auto& [rank, name, id] : node.items)
            out.items.push_back (id);
        return out;
    }

    constexpr int restRank = 1 << 20;   // "Sonstige" after every year; a song without a number after the numbered
}

juce::String albumview::albumFolder (const AlbumPlace& p)
{
    return p.year > 0 ? juce::String (p.year) + " - " + p.album : p.album;
}

juce::String albumview::songFolder (const AlbumPlace& p, const juce::String& song)
{
    return p.number > 0 ? juce::String (p.number).paddedLeft ('0', 2) + ". " + song : song;
}

FolderNode albumview::build (const std::vector<Entry>& entries, const AlbumIndex& index)
{
    Node root;
    for (const auto& e : entries)
    {
        if (e.band.isEmpty())
        {
            root.items.push_back ({ 0, e.name, e.id });
            continue;
        }
        auto& band = root.child (0, e.band);
        if (! index.hasAlbums (e.band))   // no album at all: flat, as the own folders
        {
            auto& place = e.song.isEmpty() || e.arrangement ? band : band.child (0, e.song);
            place.items.push_back ({ 0, e.name, e.id });
            continue;
        }
        const auto found = e.song.isNotEmpty() ? index.find (e.band, e.song) : std::nullopt;
        if (! found)
        {
            auto& rest = band.child (restRank, restFolder);
            auto& place = e.arrangement || e.song.isEmpty() ? rest : rest.child (0, e.song);
            place.items.push_back ({ 0, e.name, e.id });
            continue;
        }
        auto& album = band.child (found->year > 0 ? found->year : restRank - 1, albumFolder (*found));
        const int rank = found->number > 0 ? found->number : restRank;
        if (e.arrangement)
            album.items.push_back ({ rank, e.name, e.id });
        else
            album.child (rank, songFolder (*found, e.song), AlbumIndex::key (e.song))   // "Seek and Destroy" joins "Seek & Destroy"
                .items.push_back ({ 0, e.name, e.id });
    }
    return emit (root);
}

std::optional<albumview::Vote> albumview::vote (const std::vector<Vote>& votes)
{
    if (votes.empty())
        return {};
    const auto keyOf = [] (const Vote& v)   // as alben.json compares; a name without letters or digits by itself
    {
        const auto k = [] (const juce::String& s) { const auto x = AlbumIndex::key (s); return x.isNotEmpty() ? x : s.toLowerCase(); };
        return std::pair { k (v.first), k (v.second) };
    };
    std::map<std::pair<juce::String, juce::String>, int> count;
    for (const auto& v : votes)
        ++count[keyOf (v)];
    const auto best = std::max_element (count.begin(), count.end(), [] (const auto& a, const auto& b) { return a.second < b.second; });
    const auto spelled = [&] (const std::pair<juce::String, juce::String>& key)   // the spelling of the first vote with that key
    {
        for (const auto& v : votes)
            if (keyOf (v) == key)
                return v;
        return votes.front();
    };
    if (best->second * 2 > (int) votes.size())
        return spelled (best->first);
    if (std::all_of (count.begin(), count.end(), [&] (const auto& c) { return c.first.first == best->first.first; }))
        return Vote { votes.front().first, juce::String() };   // one band (as the first part spells it), no song
    return {};
}

juce::StringArray albumview::topLevel (const std::vector<Entry>& entries)
{
    juce::StringArray bands;
    for (const auto& e : entries)
        if (e.band.isNotEmpty())
            bands.addIfNotAlreadyThere (e.band, true);
    return bands;
}
