#include "app/BrowseOrder.h"

#include <algorithm>

namespace
{
    void collect (const FolderNode& node, int area, const FolderPath& path, bool hidden, std::vector<BrowseFolder>& out)
    {
        for (const auto& f : node.folders)
        {
            if (folders::countItems (f) == 0)
                continue;   // empty: not in the menu, no stop
            auto sub = path;
            sub.add (f.name);
            collect (f, area, sub, hidden, out);
        }
        if (! node.items.empty())
            out.push_back ({ area, path, node.items, hidden });
    }
}

std::vector<BrowseFolder> browseFolders (const std::vector<BrowseArea>& areas)
{
    std::vector<BrowseFolder> out;
    for (int a = 0; a < (int) areas.size(); ++a)
    {
        const auto& area = areas[(size_t) a];
        for (const auto& f : area.root.folders)
        {
            if (folders::countItems (f) == 0)
                continue;
            FolderPath top;
            top.add (f.name);
            collect (f, a, top, area.isHidden != nullptr && area.isHidden (f.name), out);
        }
        if (! area.root.items.empty())   // entries in no folder: after the folders, as in the menu
            out.push_back ({ a, {}, area.root.items, false });
    }
    return out;
}

int browseFolderOf (const std::vector<BrowseFolder>& order, const std::string& id)
{
    for (int i = 0; i < (int) order.size(); ++i)
        if (std::find (order[(size_t) i].ids.begin(), order[(size_t) i].ids.end(), id) != order[(size_t) i].ids.end())
            return i;
    return -1;
}

int nextBrowseFolder (const std::vector<BrowseFolder>& order, int from, int dir)
{
    for (int i = from + dir; i >= 0 && i < (int) order.size(); i += dir)
        if (! order[(size_t) i].hidden && ! order[(size_t) i].ids.empty())
            return i;
    return -1;
}
