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
    auto item = [&] (juce::PopupMenu& menu, int i, bool sectionOnly = false)
    {
        const auto& p = lib.get (i);
        const bool fav = lib.isFavorite (p.id);
        juce::String title (p.name);
        if (sectionOnly)
        {
            const auto prefix = Library::bandOf (p) + " – " + Library::groupOf (p) + " – ";
            if (title.startsWith (prefix))
                title = title.substring (prefix.length());
        }
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
    section ("Songs (Abläufe)"_u, [&] (int i) { return lib.isSong (i); });

    // Own patterns: band -> full song title -> building blocks.
    std::map<juce::String, std::map<juce::String, std::vector<int>>> bands;
    for (int i = 0; i < lib.size(); ++i)
        if (lib.isUserPattern (i))
            bands[Library::bandOf (lib.get (i))][Library::groupOf (lib.get (i))].push_back (i);
    if (! bands.empty())
        menu.addSectionHeader ("Eigene Patterns (Bausteine)");
    for (auto& [band, songs] : bands)
    {
        juce::PopupMenu bandMenu;
        bool bandHasCurrent = false;
        for (auto& [song, indices] : songs)
        {
            juce::PopupMenu songMenu;
            bool songHasCurrent = false;
            for (int i : indices)
            {
                item (songMenu, i, true);
                songHasCurrent = songHasCurrent || i == current;
            }
            bandMenu.addSubMenu (song, songMenu, true, nullptr, songHasCurrent);
            bandHasCurrent = bandHasCurrent || songHasCurrent;
        }
        menu.addSubMenu (band, bandMenu, true, nullptr, bandHasCurrent);
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
