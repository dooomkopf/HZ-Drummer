#include "app/NavigationMenus.h"

#include "app/Library.h"
#include "app/SearchIndex.h"
#include "app/MeterLibrary.h"
#include "app/PreviewMenuItem.h"
#include "app/StarButton.h"
#include "app/Text.h"

#include <map>

namespace
{
    const juce::String star (juce::CharPointer_UTF8 ("  \xe2\x98\x85"));

    // A reddish line at the foot of the menu that names a key; it cannot be clicked or highlighted
    class HintMenuItem : public juce::PopupMenu::CustomComponent
    {
    public:
        explicit HintMenuItem (juce::String t) : juce::PopupMenu::CustomComponent (false), text (std::move (t)) {}

        void getIdealSize (int& w, int& h) override { w = 220; h = 22; }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colour (0xffe07a70));
            g.setFont (juce::FontOptions (14.0f));
            g.drawFittedText (text, getLocalBounds().reduced (26, 0), juce::Justification::centredLeft, 1);
        }

    private:
        juce::String text;
    };

    void addHint (juce::PopupMenu& menu, int id, const juce::String& text)
    {
        juce::PopupMenu::Item it;
        it.setID (id);
        it.setEnabled (false);
        it.setCustomComponent (new HintMenuItem (text));
        menu.addItem (std::move (it));
    }
}

juce::PopupMenu buildPatternMenu (const Library& lib, int current, const PatternMenuActions& actions)
{
    auto item = [&] (juce::PopupMenu& menu, int i, const FolderPath& folder = {})
    {
        const auto& p = lib.get (i);
        const bool fav = lib.isFavorite (p.id);
        const auto title = folders::shortName (juce::String (p.name), folder);
        // custom item: hover + P auditions it (see PreviewMenuItem); click/Enter selects as usual
        const auto text = title + "   " + Library::meterText (p) + ", " + juce::String ((int) p.tempo) + " BPM" + (fav ? star : juce::String());
        juce::PopupMenu::Item it;
        it.setID (i + 2);
        const auto selectionAllowed = std::make_shared<bool> (false);
        it.setAction ([select = actions.select, selectionAllowed, i]
        {
            if (*selectionAllowed) select (i);   // deferred JUCE actions cannot select after a right click
        });
        it.setCustomComponent (new PreviewMenuItem (text, fav ? StarButton::gold : theme::textMain, i == current,
                                                    [preview = actions.preview, i] { if (preview) preview (i); },
                                                    [isPreviewing = actions.isPreviewing, i] { return isPreviewing && isPreviewing (i); }, selectionAllowed));
        menu.addItem (std::move (it));
    };

    juce::PopupMenu menu;
    menu.addItem (juce::PopupMenu::Item ("Metronom (nur Taktart)").setID (1).setTicked (current < 0)
                      .setAction ([select = actions.select] { select (-1); }));

    // move the current song or own pattern into another folder
    if ((lib.isSong (current) || lib.isUserPattern (current)) && actions.moveTo != nullptr)
    {
        const auto id = lib.get (current).id;
        const auto root = lib.folders (Library::areaOf (id));
        FolderPath at;
        folders::locate (root, id, at);
        menu.addSubMenu ("Verschieben nach", buildMoveMenu (root, at, [moveTo = actions.moveTo, id] (const FolderPath& target)
                                                            { moveTo (id, target); }, actions.editFolders));
    }
    else
    {
        menu.addItem (juce::PopupMenu::Item ("Verschieben nach (nur Songs und eigene Patterns)").setID (100003).setEnabled (false));
    }

    auto section = [&] (const juce::String& header, auto predicate)
    {
        bool any = false;
        for (int i = 0; i < lib.size(); ++i)
            if (predicate (i))
            {
                if (! any)
                    menu.addSectionHeader (header);
                any = true;
                item (menu, i);
            }
    };
    section ("Favoriten", [&] (int i) { return lib.isFavorite (lib.get (i).id); });

    // songs and own patterns in the user's folders
    std::map<std::string, int> indexById;
    for (int i = 0; i < lib.size(); ++i)
        if (lib.isSong (i) || lib.isUserPattern (i))
            indexById[lib.get (i).id] = i;
    auto entry = [&] (juce::PopupMenu& m, const std::string& id, const FolderPath& folder)
    {
        const auto it = indexById.find (id);
        if (it == indexById.end())
            return false;
        item (m, it->second, folder);
        return it->second == current;
    };
    for (auto area : { FolderTree::Songs, FolderTree::Patterns })
    {
        const auto root = lib.folders (area);
        if (folders::countItems (root) == 0)
            continue;
        menu.addSectionHeader (area == FolderTree::Songs ? "Songs (Abläufe)"_u : juce::String ("Eigene Patterns (Bausteine)"));
        addFolderMenu (menu, root, entry);
    }

    // bundled library, one submenu per style (entries are sorted by style)
    menu.addSectionHeader ("Bibliothek");
    std::map<std::string, juce::PopupMenu> styles;
    std::map<std::string, bool> styleHasCurrent;
    std::vector<std::string> order;
    for (int i = 0; i < lib.size(); ++i)
    {
        if (lib.isUserPattern (i) || lib.isSong (i))
            continue;
        const auto& tags = lib.get (i).tags;
        const auto style = tags.empty() ? std::string ("sonstige") : tags.front();
        if (styles.count (style) == 0)
            order.push_back (style);
        item (styles[style], i);
        styleHasCurrent[style] = styleHasCurrent[style] || i == current;
    }
    for (auto& style : order)
        menu.addSubMenu (Library::styleTitle (style), styles[style], true, nullptr, styleHasCurrent[style]);

    if (! lib.errors().isEmpty())
    {
        menu.addSeparator();
        menu.addItem (juce::PopupMenu::Item (juce::String (lib.errors().size()) + " Pattern-Datei(en) fehlerhaft ...")
                          .setID (100000).setAction (actions.showErrors));
    }

    // key hints instead of a favourite toggle entry: Strg+F and the star on the button do that
    menu.addSeparator();
    addHint (menu, 100001, "Anhören (P)"_u);
    addHint (menu, 100002, "Favoriten (Strg+F)");
    return menu;
}

bool addFolderMenu (juce::PopupMenu& menu, const FolderNode& node,
                    const std::function<bool (juce::PopupMenu&, const std::string&, const FolderPath&)>& addEntry,
                    const FolderPath& path)
{
    bool hasCurrent = false;
    for (auto& f : node.folders)
    {
        if (folders::countItems (f) == 0)
            continue;
        auto sub = path;
        sub.add (f.name);
        juce::PopupMenu subMenu;
        const bool inside = addFolderMenu (subMenu, f, addEntry, sub);
        menu.addSubMenu (f.name, subMenu, true, nullptr, inside);
        hasCurrent = hasCurrent || inside;
    }
    for (auto& id : node.items)
        hasCurrent = addEntry (menu, id, path) || hasCurrent;
    return hasCurrent;
}

namespace
{
    void addMoveTargets (juce::PopupMenu& menu, const FolderNode& node, const FolderPath& path, const FolderPath& current,
                         const std::function<void (const FolderPath&)>& moveTo, int& nextId)
    {
        for (auto& f : node.folders)
        {
            auto target = path;
            target.add (f.name);
            const bool here = target == current;
            auto moveHere = [moveTo, target] { moveTo (target); };
            if (f.folders.empty())
            {
                menu.addItem (juce::PopupMenu::Item (f.name).setID (nextId++).setTicked (here).setAction (moveHere));
                continue;
            }
            juce::PopupMenu sub;
            sub.addItem (juce::PopupMenu::Item ("Hierher").setID (nextId++).setTicked (here).setAction (moveHere));
            sub.addSeparator();
            addMoveTargets (sub, f, target, current, moveTo, nextId);
            menu.addSubMenu (f.name, sub, true, nullptr, folders::startsWith (current, target));
        }
    }
}

juce::PopupMenu buildMoveMenu (const FolderNode& root, const FolderPath& current,
                               std::function<void (const FolderPath&)> moveTo, std::function<void()> editFolders)
{
    juce::PopupMenu menu;
    int nextId = 200000;   // apart from the pattern menu's own ids
    menu.addItem (juce::PopupMenu::Item ("Oberste Ebene").setID (nextId++).setTicked (current.isEmpty())
                      .setAction ([moveTo] { moveTo ({}); }));
    if (! root.folders.empty())
        menu.addSeparator();
    addMoveTargets (menu, root, {}, current, moveTo, nextId);
    if (editFolders != nullptr)
    {
        menu.addSeparator();
        menu.addItem (juce::PopupMenu::Item ("Ordner anlegen und sortieren ...").setID (nextId++).setAction (editFolders));
    }
    return menu;
}

std::vector<QuickSearch::Item> buildSearchItems (const Library& lib, const MeterLibrary& meters,
                                                 std::function<void (const std::string&)> selectPattern,
                                                 std::function<void (int)> selectMeter,
                                                 std::function<void()> selectMetronome)
{
    std::vector<QuickSearch::Item> items;

    QuickSearch::Item metronome;
    metronome.kind = "Modus";
    metronome.title = "Metronom (nur Taktart)";
    metronome.haystack = "metronom klick taktart";
    metronome.action = selectMetronome;
    items.push_back (metronome);

    SearchIndex index;
    index.build (lib);
    for (int i = 0; i < index.size(); ++i)
    {
        const auto& source = index.item (i);
        QuickSearch::Item it;
        it.kind = source.group == SearchItem::Group::Songs ? "Song"
                : source.group == SearchItem::Group::Sections ? "Abschnitt"
                : source.group == SearchItem::Group::Own ? "Eigenes" : "Pattern";
        it.title = source.title;
        it.detail = source.detail;
        it.haystack = source.haystack;
        it.favorite = source.favorite;
        it.tempo = source.tempo;
        it.meters = source.meterNumbers;
        it.colour = source.colour;
        it.shape = source.shape;
        it.action = [selectPattern, id = source.id] { selectPattern (id); };
        items.push_back (std::move (it));
    }

    for (int i = 0; i < meters.size(); ++i)
    {
        QuickSearch::Item it;
        it.kind = "Taktart";
        it.meters = { { meters.get (i).numerator, meters.get (i).denominator } };
        it.title = meters.label (i);
        it.detail = meters.isCustom (i) ? "eigene" : "";
        it.haystack = it.title + " taktart metronom";
        it.favorite = meters.isFavorite (i);
        it.action = [selectMeter, i] { selectMeter (i); };
        items.push_back (std::move (it));
    }
    return items;
}
