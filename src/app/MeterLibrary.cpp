#include "app/MeterLibrary.h"
#include "app/Text.h"
#include "app/views/InWindowDialog.h"

#include "engine/MeterCatalog.h"

#include <algorithm>

namespace
{
    constexpr int maxRecent = 8;

    // Menu IDs: meters 1..capacity, special commands above
    constexpr int idCustom = 100000;
    constexpr int idToggleFavorite = 100001;

}

MeterLibrary::MeterLibrary()
{
    for (const auto& m : hz::buildMeterCatalog())
        if (count < capacity)
            table[(size_t) count++] = m;
    catalogSize = count;

    // The user sets favorites themselves (Ctrl+F); none by default
}

juce::String MeterLibrary::label (int index) const
{
    return hz::meterLabel (get (index));
}

int MeterLibrary::indexOf (const hz::Meter& m) const
{
    for (int i = 0; i < count; ++i)
        if (hz::sameMeter (table[(size_t) i], m))
            return i;
    return -1;
}

int MeterLibrary::findOrAdd (const hz::Meter& m)
{
    if (const int i = indexOf (m); i >= 0)
        return i;

    if (count >= capacity || ! m.isValid())
        return -1;

    table[(size_t) count] = m;
    return count++;
}

int MeterLibrary::step (int current, int dir) const
{
    return ((current + dir) % count + count) % count;
}

int MeterLibrary::stepFavorite (int current, int dir) const
{
    if (favorites.empty())
        return current;

    if (dir > 0)
    {
        for (int f : favorites)
            if (f > current)
                return f;
        return favorites.front();
    }

    for (auto it = favorites.rbegin(); it != favorites.rend(); ++it)
        if (*it < current)
            return *it;
    return favorites.back();
}

bool MeterLibrary::isFavorite (int index) const
{
    return std::binary_search (favorites.begin(), favorites.end(), index);
}

void MeterLibrary::toggleFavorite (int index)
{
    auto it = std::lower_bound (favorites.begin(), favorites.end(), index);
    if (it != favorites.end() && *it == index)
        favorites.erase (it);
    else
        favorites.insert (it, index);
}

void MeterLibrary::noteUsed (int index)
{
    recent.erase (std::remove (recent.begin(), recent.end(), index), recent.end());
    recent.insert (recent.begin(), index);
    if ((int) recent.size() > maxRecent)
        recent.resize ((size_t) maxRecent);
}

juce::PopupMenu MeterLibrary::buildMenu (int current, MenuCallbacks cb) const
{
    auto item = [&] (juce::PopupMenu& menu, int index, const juce::String& text)
    {
        juce::PopupMenu::Item it (text + (isFavorite (index) ? juce::String (juce::CharPointer_UTF8 ("  \xe2\x98\x85")) : juce::String()));
        it.setID (index + 1);
        if (isFavorite (index))
            it.setColour (juce::Colour (0xffffc23d));   // favorites in gold
        it.setTicked (index == current);
        it.setAction ([cb, index] { cb.select (index); });
        menu.addItem (std::move (it));
    };

    juce::PopupMenu menu;
    if (cb.hint.isNotEmpty())
    {
        menu.addSectionHeader (cb.hint);
        menu.addSeparator();
    }

    menu.addSectionHeader ("Favoriten");
    for (int f : favorites)
        item (menu, f, label (f));

    if (! recent.empty())
    {
        juce::PopupMenu sub;
        for (int r : recent)
            item (sub, r, label (r));
        menu.addSubMenu ("Zuletzt benutzt", sub);
    }

    menu.addSeparator();

    // Catalog: denominator -> (for x/8, x/16) numerator -> groupings
    struct Family { int denominator; const char* title; };
    for (const auto& fam : { Family { 4, "Viertel (x/4)" }, Family { 8, "Achtel (x/8)" }, Family { 16, "Sechzehntel (x/16)" } })
    {
        juce::PopupMenu famMenu;
        int i = 0;
        while (i < catalogSize)
        {
            const auto& m = table[(size_t) i];
            if (m.denominator != fam.denominator) { ++i; continue; }

            // collect all entries with the same numerator
            int end = i;
            while (end < catalogSize && table[(size_t) end].denominator == m.denominator && table[(size_t) end].numerator == m.numerator)
                ++end;

            const juce::String name = juce::String (m.numerator) + "/" + juce::String (m.denominator);
            if (end - i == 1 && m.numGroups == m.numerator)
            {
                item (famMenu, i, name);
            }
            else
            {
                juce::PopupMenu groups;
                bool containsCurrent = false;
                for (int k = i; k < end; ++k)
                {
                    item (groups, k, juce::String (hz::groupingLabel (table[(size_t) k])));
                    containsCurrent = containsCurrent || k == current;
                }
                famMenu.addSubMenu (name, groups, true, nullptr, containsCurrent);
            }
            i = end;
        }
        menu.addSubMenu (fam.title, famMenu);
    }

    if (count > catalogSize)
    {
        juce::PopupMenu own;
        for (int i = catalogSize; i < count; ++i)
            item (own, i, label (i));
        menu.addSubMenu ("Eigene", own);
    }

    menu.addSeparator();
    menu.addItem (juce::PopupMenu::Item ("Eigene Taktart eingeben ...").setID (idCustom).setAction (cb.custom));
    menu.addItem (juce::PopupMenu::Item (isFavorite (current) ? "Aktuelle aus Favoriten entfernen   (Strg+F)" : "Aktuelle zu Favoriten hinzufügen   (Strg+F)"_u)
                      .setID (idToggleFavorite)
                      .setAction ([cb, current] { cb.toggleFavorite (current); }));
    return menu;
}

juce::String MeterLibrary::key (const hz::Meter& m)
{
    juce::String k = juce::String (m.numerator) + "/" + juce::String (m.denominator);
    if (m.numGroups != m.numerator)
        k << ":" << juce::String (hz::groupingLabel (m));
    return k;
}

int MeterLibrary::fromKey (const juce::String& k)
{
    hz::Meter m;
    if (! hz::parseMeter (k.toStdString(), m))
        return -1;
    return findOrAdd (m);
}

juce::StringArray MeterLibrary::keysOf (const std::vector<int>& indices) const
{
    juce::StringArray out;
    for (int i : indices)
        out.add (key (get (i)));
    return out;
}

std::vector<int> MeterLibrary::indicesOf (const juce::StringArray& keys)
{
    std::vector<int> out;
    for (auto& k : keys)
        if (const int i = fromKey (k); i >= 0 && std::find (out.begin(), out.end(), i) == out.end())
            out.push_back (i);
    return out;
}

void MeterLibrary::save (juce::PropertiesFile& props) const
{
    std::vector<int> custom;
    for (int i = catalogSize; i < count; ++i)
        custom.push_back (i);

    props.setValue ("meterCustom", keysOf (custom).joinIntoString (";"));
    props.setValue ("meterFavorites", keysOf (favorites).joinIntoString (";"));
    props.setValue ("meterRecent", keysOf (recent).joinIntoString (";"));
}

void MeterLibrary::load (juce::PropertiesFile& props)
{
    auto split = [] (const juce::String& s) { return juce::StringArray::fromTokens (s, ";", ""); };

    indicesOf (split (props.getValue ("meterCustom")));   // create custom ones first

    if (props.containsKey ("meterFavorites"))
    {
        favorites = indicesOf (split (props.getValue ("meterFavorites")));
        std::sort (favorites.begin(), favorites.end());
    }
    recent = indicesOf (split (props.getValue ("meterRecent")));
}

void MeterLibrary::askCustomMeter (juce::Component* parent, int current, std::function<void (int)> onSelected)
{
    auto* w = new InWindowDialog ("Eigene Taktart",
                                     "Zähler/Nenner und optional die Gruppierung, z.B.  17/16 4+4+4+5  oder  7/4"_u,
                                     juce::MessageBoxIconType::NoIcon, parent);
    w->addTextEditor ("meter", key (get (juce::jmax (0, current))).replace (":", " "), "Taktart");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Abbrechen", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w, onSelected] (int result)
    {
        if (result != 1)
            return;

        hz::Meter m;
        const auto text = w->getTextEditorContents ("meter");
        if (! hz::parseMeter (text.toStdString(), m))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Ungültige Taktart"_u,
                "\"" + text + "\" passt nicht.\nDie Gruppen müssen zusammen den Zähler ergeben (z.B. 7/8 = 2+2+3), "_u
                "erlaubte Nenner: 1, 2, 4, 8, 16, 32."_u);
            return;
        }

        if (const int i = findOrAdd (m); i >= 0)
            onSelected (i);
    }), true);
}
