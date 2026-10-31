#include "app/FolderSort.h"

#include "app/Text.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace
{
    using Label = std::function<juce::String (const std::string&, const FolderPath&)>;

    // how a name sorts: without case, umlauts as their base letter, without signs in front
    juce::String sortKey (const juce::String& name)
    {
        const auto key = name.toLowerCase().replace ("ä"_u, "a").replace ("ö"_u, "o").replace ("ü"_u, "u").replace ("ß"_u, "ss");
        int start = 0;
        while (start < key.length() && ! juce::CharacterFunctions::isLetterOrDigit (key[start]))
            ++start;
        return key.substring (start);
    }

    void sortNode (FolderNode& node, const FolderPath& path, bool deep, const Label& label)
    {
        std::stable_sort (node.folders.begin(), node.folders.end(),
                          [] (const FolderNode& a, const FolderNode& b) { return folders::sortsBefore (a.name, b.name); });
        std::vector<std::pair<juce::String, std::string>> named;
        for (const auto& id : node.items)
            named.emplace_back (label (id, path), id);
        std::stable_sort (named.begin(), named.end(), [] (const auto& a, const auto& b) { return folders::sortsBefore (a.first, b.first); });
        for (size_t i = 0; i < named.size(); ++i)
            node.items[i] = named[i].second;
        if (deep)
            for (auto& f : node.folders)
            {
                auto sub = path;
                sub.add (f.name);
                sortNode (f, sub, true, label);
            }
    }
}

bool folders::sortsBefore (const juce::String& a, const juce::String& b)
{
    const int c = sortKey (a).compareNatural (sortKey (b));
    return c != 0 ? c < 0 : a.compareNatural (b, true) < 0;   // alike apart from case or signs: still a fixed order
}

juce::String folders::sortFolder (FolderNode& root, const FolderPath& path, bool withSubfolders, const Label& label)
{
    auto* node = const_cast<FolderNode*> (find (root, path));   // root itself is not const
    if (node == nullptr)
        return "Ordner nicht gefunden";
    sortNode (*node, path, withSubfolders, label);
    return {};
}
