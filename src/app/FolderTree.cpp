#include "app/FolderTree.h"
#include "app/Text.h"

#include <algorithm>
#include <set>

namespace
{
    constexpr int maxDepth = 24;         // a hand-edited file must not nest without end
    constexpr int maxNameLength = 60;
    const char* const areaKeys[] = { "songs", "patterns", "library" };

    FolderNode* child (FolderNode& node, const juce::String& name)
    {
        for (auto& f : node.folders)
            if (f.name.equalsIgnoreCase (name))
                return &f;
        return nullptr;
    }

    FolderNode* findMutable (FolderNode& root, const FolderPath& path)
    {
        return const_cast<FolderNode*> (folders::find (root, path));
    }

    FolderPath parentOf (const FolderPath& path)
    {
        auto parent = path;
        parent.removeRange (path.size() - 1, 1);
        return parent;
    }

    // the folder at path, created where missing
    FolderNode& folderAt (FolderNode& root, const FolderPath& path)
    {
        auto* node = &root;
        for (auto& part : path)
        {
            const auto name = part.trim();
            if (name.isEmpty())
                continue;
            auto* next = child (*node, name);
            if (next == nullptr)
            {
                node->folders.push_back ({ name, {}, {} });
                next = &node->folders.back();
            }
            node = next;
        }
        return *node;
    }

    void merge (FolderNode& into, FolderNode&& from)
    {
        for (auto& f : from.folders)
        {
            if (auto* same = child (into, f.name))
                merge (*same, std::move (f));
            else
                into.folders.push_back (std::move (f));
        }
        into.items.insert (into.items.end(), from.items.begin(), from.items.end());
    }

    // drops ids of entries that are gone and second mentions of an id
    void prune (FolderNode& node, const std::set<std::string>& known, std::set<std::string>& placed)
    {
        std::vector<std::string> kept;
        for (auto& id : node.items)
            if (known.count (id) > 0 && placed.insert (id).second)
                kept.push_back (id);
        node.items = std::move (kept);
        for (auto& f : node.folders)
            prune (f, known, placed);
    }

    bool pathLess (const FolderPath& a, const FolderPath& b)
    {
        for (int i = 0; i < juce::jmin (a.size(), b.size()); ++i)
            if (const int c = a[i].compareNatural (b[i], false); c != 0)
                return c < 0;
        return a.size() < b.size();
    }

    FolderNode fromVar (const juce::var& v, int depth)
    {
        FolderNode node;
        node.name = v["name"].toString().trim();
        if (auto* items = v["items"].getArray())
            for (auto& id : *items)
                if (id.isString() && id.toString().isNotEmpty())
                    node.items.push_back (id.toString().toStdString());
        if (auto* list = v["folders"].getArray(); list != nullptr && depth < maxDepth)
            for (auto& f : *list)
            {
                auto sub = fromVar (f, depth + 1);
                if (sub.name.isEmpty())
                    continue;
                if (auto* same = child (node, sub.name))
                    merge (*same, std::move (sub));
                else
                    node.folders.push_back (std::move (sub));
            }
        return node;
    }

    juce::var toVar (const FolderNode& node, bool isRoot)
    {
        auto* o = new juce::DynamicObject();
        if (! isRoot)
            o->setProperty ("name", node.name);
        juce::Array<juce::var> list, items;
        for (auto& f : node.folders)
            list.add (toVar (f, false));
        for (auto& id : node.items)
            items.add (juce::String (id));
        o->setProperty ("folders", list);
        o->setProperty ("items", items);
        return juce::var (o);
    }

    juce::String checkName (const FolderNode& parent, const juce::String& name, const FolderNode* except)
    {
        if (name.isEmpty())
            return "Der Ordner braucht einen Namen.";
        if (name.length() > maxNameLength)
            return "Der Name ist zu lang, höchstens "_u + juce::String (maxNameLength) + " Zeichen.";
        for (auto& f : parent.folders)
            if (&f != except && f.name.equalsIgnoreCase (name))
                return "Hier gibt es schon einen Ordner \"" + f.name + "\".";
        return {};
    }

    template <typename T>
    void moveWithin (std::vector<T>& list, int from, int index)
    {
        auto value = std::move (list[(size_t) from]);
        list.erase (list.begin() + from);
        if (index > from)
            --index;   // counted in the list before the move
        index = juce::jlimit (0, (int) list.size(), index);
        list.insert (list.begin() + index, std::move (value));
    }

    const juce::String notFound = "Den Ordner gibt es nicht mehr.";
}

//==============================================================================
FolderNode FolderTree::resolve (Area area, const std::vector<Entry>& entries) const
{
    std::set<std::string> known;
    for (auto& e : entries)
        known.insert (e.id);

    auto root = roots[(size_t) area];
    std::set<std::string> placed;
    prune (root, known, placed);

    // the rest at their default places; sorted by path, so new folders come in alphabetical order
    std::vector<const Entry*> rest;
    for (auto& e : entries)
        if (placed.insert (e.id).second)
            rest.push_back (&e);
    std::stable_sort (rest.begin(), rest.end(), [] (const Entry* a, const Entry* b) { return pathLess (a->defaultPath, b->defaultPath); });
    for (auto* e : rest)
        folderAt (root, e->defaultPath).items.push_back (e->id);
    return root;
}

juce::String FolderTree::load (const juce::File& file)
{
    roots = {};
    unreadable = false;
    if (! file.existsAsFile())
        return {};

    juce::var json;
    if (juce::JSON::parse (file.loadFileAsString(), json).failed() || ! json.isObject() || (int) json["formatVersion"] != 1)
    {
        unreadable = true;
        return file.getFileName() + " ist nicht lesbar, die Ordner fehlen. Die Datei bleibt unverändert."_u;
    }
    for (int a = 0; a < NumAreas; ++a)
    {
        roots[(size_t) a] = fromVar (json[areaKeys[a]], 0);
        roots[(size_t) a].name = {};
    }
    return {};
}

juce::String FolderTree::save (const juce::File& file) const
{
    if (unreadable)
        return file.getFileName() + " war nicht lesbar und wird nicht überschrieben. Bitte die Datei prüfen."_u;

    auto* root = new juce::DynamicObject();
    root->setProperty ("formatVersion", 1);
    for (int a = 0; a < NumAreas; ++a)
        root->setProperty (areaKeys[a], toVar (roots[(size_t) a], true));

    juce::TemporaryFile temp (file, juce::TemporaryFile::useHiddenFile);   // hidden: a leftover after a crash is never loaded
    if (! file.getParentDirectory().createDirectory()
        || ! temp.getFile().replaceWithText (juce::JSON::toString (juce::var (root)))
        || ! temp.overwriteTargetFileWithTemporary())
        return "Ordner nicht speicherbar: "_u + file.getFullPathName();
    return {};
}

//==============================================================================
namespace folders
{
    const FolderNode* find (const FolderNode& root, const FolderPath& path)
    {
        auto* node = &root;
        for (auto& name : path)
        {
            node = child (const_cast<FolderNode&> (*node), name);
            if (node == nullptr)
                return nullptr;
        }
        return node;
    }

    bool locate (const FolderNode& node, const std::string& id, FolderPath& folder)
    {
        if (std::find (node.items.begin(), node.items.end(), id) != node.items.end())
            return true;
        for (auto& f : node.folders)
        {
            folder.add (f.name);
            if (locate (f, id, folder))
                return true;
            folder.removeRange (folder.size() - 1, 1);
        }
        return false;
    }

    int countItems (const FolderNode& node)
    {
        int n = (int) node.items.size();
        for (auto& f : node.folders)
            n += countItems (f);
        return n;
    }

    bool startsWith (const FolderPath& path, const FolderPath& prefix)
    {
        if (path.size() < prefix.size())
            return false;
        for (int i = 0; i < prefix.size(); ++i)
            if (! path[i].equalsIgnoreCase (prefix[i]))
                return false;
        return true;
    }

    juce::String addFolder (FolderNode& root, const FolderPath& parentPath, const juce::String& rawName)
    {
        auto* parent = findMutable (root, parentPath);
        if (parent == nullptr)
            return notFound;
        if (parentPath.size() >= maxDepth)
            return "Tiefer lassen sich Ordner nicht verschachteln.";
        const auto name = rawName.trim();
        if (const auto error = checkName (*parent, name, nullptr); error.isNotEmpty())
            return error;
        parent->folders.push_back ({ name, {}, {} });
        return {};
    }

    juce::String renameFolder (FolderNode& root, const FolderPath& path, const juce::String& rawName)
    {
        auto* node = path.isEmpty() ? nullptr : findMutable (root, path);
        if (node == nullptr)
            return notFound;
        const auto name = rawName.trim();
        if (name.isEmpty())
            return "Der Ordner braucht einen Namen.";
        if (name.length() > maxNameLength)
            return "Der Name ist zu lang, höchstens "_u + juce::String (maxNameLength) + " Zeichen.";

        auto& parent = *findMutable (root, parentOf (path));
        auto same = std::find_if (parent.folders.begin(), parent.folders.end(),
                                  [&] (const FolderNode& f) { return &f != node && f.name.equalsIgnoreCase (name); });
        if (same == parent.folders.end())
        {
            node->name = name;   // also just another spelling of the same name
            return {};
        }

        // a folder of that name exists already: the two become one, under the new spelling
        same->name = name;
        auto moved = std::move (*node);
        parent.folders.erase (parent.folders.begin() + (node - parent.folders.data()));
        merge (*child (parent, name), std::move (moved));
        return {};
    }

    juce::String removeFolder (FolderNode& root, const FolderPath& path)
    {
        auto* node = path.isEmpty() ? nullptr : find (root, path);
        if (node == nullptr)
            return notFound;
        if (countItems (*node) > 0)
            return "Der Ordner ist nicht leer. Zuerst die Einträge woandershin verschieben."_u;
        auto& siblings = findMutable (root, parentOf (path))->folders;
        siblings.erase (std::find_if (siblings.begin(), siblings.end(), [node] (const FolderNode& f) { return &f == node; }));
        return {};
    }

    juce::String moveFolder (FolderNode& root, const FolderPath& path, const FolderPath& newParent, int index)
    {
        auto* node = path.isEmpty() ? nullptr : find (root, path);
        if (node == nullptr || find (root, newParent) == nullptr)
            return notFound;
        if (startsWith (newParent, path))
            return "Ein Ordner kann nicht in sich selbst liegen.";

        auto& siblings = findMutable (root, parentOf (path))->folders;
        const int from = (int) (node - siblings.data());
        if (startsWith (parentOf (path), newParent) && newParent.size() == path.size() - 1)
        {
            if (index >= 0)
                moveWithin (siblings, from, index);
            return {};   // same parent and no position: stays where it is
        }

        if (const auto error = checkName (*find (root, newParent), node->name, nullptr); error.isNotEmpty())
            return error;
        auto moved = std::move (siblings[(size_t) from]);
        siblings.erase (siblings.begin() + from);
        auto& target = findMutable (root, newParent)->folders;   // found again: the erase may have moved it
        const int at = index < 0 ? (int) target.size() : juce::jlimit (0, (int) target.size(), index);
        target.insert (target.begin() + at, std::move (moved));
        return {};
    }

    juce::String moveItem (FolderNode& root, const std::string& id, const FolderPath& targetPath, int index)
    {
        FolderPath from;
        if (! locate (root, id, from))
            return "Den Eintrag gibt es nicht mehr.";
        auto* source = findMutable (root, from);
        auto* target = findMutable (root, targetPath);
        if (target == nullptr)
            return notFound;

        const int old = (int) (std::find (source->items.begin(), source->items.end(), id) - source->items.begin());
        if (source == target)
        {
            if (index >= 0)
                moveWithin (source->items, old, index);
            return {};
        }
        source->items.erase (source->items.begin() + old);
        const int at = index < 0 ? (int) target->items.size() : juce::jlimit (0, (int) target->items.size(), index);
        target->items.insert (target->items.begin() + at, id);
        return {};
    }

    bool shiftFolder (FolderNode& root, const FolderPath& path, int delta)
    {
        auto* node = path.isEmpty() ? nullptr : find (root, path);
        if (node == nullptr)
            return false;
        auto& siblings = findMutable (root, parentOf (path))->folders;
        const int from = (int) (node - siblings.data()), to = from + delta;
        if (to < 0 || to >= (int) siblings.size())
            return false;
        std::swap (siblings[(size_t) from], siblings[(size_t) to]);
        return true;
    }

    bool shiftItem (FolderNode& root, const std::string& id, int delta)
    {
        FolderPath at;
        if (! locate (root, id, at))
            return false;
        auto& items = findMutable (root, at)->items;
        const int from = (int) (std::find (items.begin(), items.end(), id) - items.begin()), to = from + delta;
        if (to < 0 || to >= (int) items.size())
            return false;
        std::swap (items[(size_t) from], items[(size_t) to]);
        return true;
    }

    juce::String shortName (const juce::String& name, const FolderPath& folder)
    {
        const juce::String dash (juce::CharPointer_UTF8 (" \xe2\x80\x93 "));
        for (int first = 0; first < folder.size(); ++first)   // the longest tail of the path first
        {
            FolderPath tail;
            for (int i = first; i < folder.size(); ++i)
                tail.add (folder[i]);
            const auto prefix = tail.joinIntoString (dash) + dash;
            if (name.startsWithIgnoreCase (prefix) && name.length() > prefix.length())
                return name.substring (prefix.length());
        }
        return name;
    }
}
