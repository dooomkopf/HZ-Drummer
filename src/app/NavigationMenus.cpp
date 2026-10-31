#include "app/NavigationMenus.h"

#include "app/Library.h"
#include "app/SearchIndex.h"
#include "app/MeterLibrary.h"
#include "app/PreviewMenuItem.h"
#include "app/StarButton.h"
#include "app/Text.h"

#include <algorithm>
#include <map>

namespace
{
    const juce::String star (juce::CharPointer_UTF8 ("  \xe2\x98\x85"));

    // A reddish line at the foot of the menu that names a key; it cannot be clicked or highlighted
    class HintMenuItem : public juce::PopupMenu::CustomComponent
    {
    public:
        explicit HintMenuItem (juce::String t) : juce::PopupMenu::CustomComponent (false), text (std::move (t)) {}

        void getIdealSize (int& w, int& h) override
        {
            w = juce::jmax (220, 52 + (int) std::ceil (juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (14.0f)), text)));
            h = 22;
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colour (0xffe07a70));
            g.setFont (juce::FontOptions (14.0f));
            g.drawFittedText (text, getLocalBounds().reduced (26, 0), juce::Justification::centredLeft, 1);
        }

    private:
        juce::String text;
    };

    // runs after the menu has closed: an action from inside the menu must not delete it under itself
    std::function<void()> afterClosingMenus (std::function<void()> action)
    {
        return [action = std::move (action)]
        {
            juce::MessageManager::callAsync ([action]
            {
                juce::PopupMenu::dismissAllActiveMenus();
                action();
            });
        };
    }

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
        std::function<void()> context;
        if (actions.rename)
            context = afterClosingMenus ([rename = actions.rename, id = p.id] { rename (id); });   // bundled: says why not
        it.setCustomComponent (new PreviewMenuItem (text, fav ? StarButton::gold : theme::textMain, i == current,
                                                    [preview = actions.preview, i] { if (preview) preview (i); },
                                                    [isPreviewing = actions.isPreviewing, i] { return isPreviewing && isPreviewing (i); },
                                                    selectionAllowed, std::move (context)));
        menu.addItem (std::move (it));
    };

    juce::PopupMenu menu;
    menu.addItem (juce::PopupMenu::Item ("Metronom (nur Taktart)").setID (1).setTicked (current < 0)
                      .setAction ([select = actions.select] { select (-1); }));

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

    // songs, own patterns and the bundled library in the user's folders
    std::map<std::string, int> indexById;
    for (int i = 0; i < lib.size(); ++i)
        indexById[lib.get (i).id] = i;
    auto entry = [&] (juce::PopupMenu& m, const std::string& id, const FolderPath& folder)
    {
        const auto it = indexById.find (id);
        if (it == indexById.end())
            return false;
        item (m, it->second, folder);
        return it->second == current;
    };
    for (auto area : { FolderTree::Songs, FolderTree::Patterns, FolderTree::Bundled })
    {
        auto root = lib.folders (area);
        // folders switched off under System → Menü-Ordner stay out of the menu (the search still finds their entries)
        root.folders.erase (std::remove_if (root.folders.begin(), root.folders.end(),
                                            [&] (const FolderNode& f) { return lib.folderHidden (area, f.name); }),
                            root.folders.end());
        if (folders::countItems (root) == 0)
            continue;
        menu.addSectionHeader (area == FolderTree::Songs ? "Arrangements (Songs)"_u
                               : area == FolderTree::Patterns ? juce::String ("Bausteine (Pattern)") : juce::String ("Bibliothek-Bausteine"));
        addFolderMenu (menu, root, entry);
    }

    if (! lib.errors().isEmpty())
    {
        menu.addSeparator();
        menu.addItem (juce::PopupMenu::Item (juce::String (lib.errors().size()) + " Baustein-Datei(en) fehlerhaft ...")
                          .setID (100000).setAction (actions.showErrors));
    }

    // key hints instead of a favourite toggle entry: Strg+F and the star on the button do that
    menu.addSeparator();
    addHint (menu, 100001, "Play/Pause Sample: Mausover + P");
    addHint (menu, 100002, "Toggle Favorit: Strg+F");
    addHint (menu, 100003, "Umbenennen (Baustein): Rechtsklick");
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

std::vector<QuickSearch::Item> buildSearchItems (const Library& lib, const MeterLibrary& meters,
                                                 std::function<void (const std::string&)> selectPattern,
                                                 std::function<void (int)> selectMeter,
                                                 std::function<void()> selectMetronome,
                                                 std::function<void (const std::string&)> previewPattern,
                                                 std::function<bool (const std::string&)> isPreviewing)
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
        it.kind = source.group == SearchItem::Group::Songs ? "Arrangement"
                : source.group == SearchItem::Group::Own ? "Pattern" : "Bibliothek";
        it.title = source.title;
        it.detail = source.detail;
        it.haystack = source.haystack;
        it.favorite = source.favorite;
        it.tempo = source.tempo;
        it.meters = source.meterNumbers;
        it.colour = source.colour;
        it.shape = source.shape;
        it.action = [selectPattern, id = source.id] { selectPattern (id); };
        if (previewPattern)
            it.preview = [previewPattern, id = source.id] { previewPattern (id); };
        if (isPreviewing)
            it.isPreviewing = [isPreviewing, id = source.id] { return isPreviewing (id); };
        items.push_back (std::move (it));
    }

    for (int i = 0; i < meters.size(); ++i)
    {
        QuickSearch::Item it;
        it.kind = "Metronom";   // what it selects; the second line says what the title is (and keeps the rows even)
        it.meters = { { meters.get (i).numerator, meters.get (i).denominator } };
        it.title = meters.label (i);
        it.detail = meters.isCustom (i) ? "eigene Taktart" : "Taktart";
        it.haystack = it.title + " taktart metronom";
        it.favorite = meters.isFavorite (i);
        it.action = [selectMeter, i] { selectMeter (i); };
        items.push_back (std::move (it));
    }
    return items;
}
