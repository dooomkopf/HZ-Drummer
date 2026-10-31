#include "app/MenuFolderSlots.h"

namespace
{
    const char* const areaKeys[] = { "arrangements", "bausteine", "bibliothek" };   // FolderTree::Area order

    MenuFolderSlot preset (const juce::String& name, bool songs, bool patterns, bool library)
    {
        MenuFolderSlot s;
        s.name = name;
        s.all = { songs, patterns, library };   // an area not "all" keeps an empty list: none of its folders
        return s;
    }

    MenuFolderSlot fixedSlot (int index)
    {
        if (index == menufolderslots::maximal)
            return preset ("Maximal", true, true, true);
        auto s = preset ("Minimal", false, false, true);   // the library and the examples of the start package
        s.shown[FolderTree::Songs].add ("Beispiele");
        return s;
    }

    menufolderslots::Slots presets()
    {
        menufolderslots::Slots slots;
        slots[7] = preset ("Nur Bausteine", false, true, true);
        slots[8] = preset ("Nur Arrangements", true, false, false);
        slots[menufolderslots::minimal] = fixedSlot (menufolderslots::minimal);
        slots[menufolderslots::maximal] = fixedSlot (menufolderslots::maximal);
        return slots;
    }
}

namespace menufolderslots
{
bool isFixed (int index)
{
    return index == minimal || index == maximal;
}

juce::File file()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/HZ-Drummer/menue-ordner.json");
}

Slots load (const juce::File& f)
{
    const auto root = juce::JSON::parse (f);
    const auto* list = root["plaetze"].getArray();
    if (list == nullptr)
        return presets();
    Slots slots;
    for (const auto& v : *list)
    {
        const int index = (int) v["platz"] - 1;
        if (index < 0 || index >= count || isFixed (index))
            continue;
        auto& s = slots[(size_t) index];
        s.name = v["name"].toString().trim();
        if (s.isFree())
            continue;
        for (int a = 0; a < FolderTree::NumAreas; ++a)
        {
            const auto value = v[areaKeys[a]];
            s.all[(size_t) a] = value.toString() == "alle";
            if (const auto* names = value.getArray())
                for (const auto& n : *names)
                    s.shown[(size_t) a].addIfNotAlreadyThere (n.toString(), true);
        }
    }
    slots[minimal] = fixedSlot (minimal);
    slots[maximal] = fixedSlot (maximal);
    return slots;
}

bool save (const Slots& slots, const juce::File& f)
{
    juce::Array<juce::var> list;
    for (int i = 0; i < count; ++i)
    {
        if (isFixed (i))
            continue;
        const auto& s = slots[(size_t) i];
        auto* o = new juce::DynamicObject();
        o->setProperty ("platz", i + 1);
        o->setProperty ("name", s.name);
        if (! s.isFree())
            for (int a = 0; a < FolderTree::NumAreas; ++a)
            {
                juce::Array<juce::var> names;
                for (const auto& n : s.shown[(size_t) a])
                    names.add (n);
                o->setProperty (areaKeys[a], s.all[(size_t) a] ? juce::var ("alle") : juce::var (names));
            }
        list.add (juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("plaetze", list);
    if (f.getParentDirectory().createDirectory().failed())
        return false;
    juce::TemporaryFile temp (f, juce::TemporaryFile::useHiddenFile);   // hidden: a leftover after a crash is never loaded
    {
        // written and flushed by hand, as trainer.json: a full disk must not leave a half file in place
        juce::FileOutputStream out (temp.getFile());
        if (! out.openedOk() || ! out.writeText (juce::JSON::toString (juce::var (root)), false, false, "\n"))
            return false;
        out.flush();
        if (out.getStatus().failed())
            return false;
    }
    return temp.overwriteTargetFileWithTemporary();
}

MenuFolderSlot capture (const juce::String& name, const AreaNames& folders, const AreaNames& hidden)
{
    MenuFolderSlot s;
    s.name = name;
    for (int a = 0; a < FolderTree::NumAreas; ++a)
    {
        juce::StringArray visible;
        for (const auto& f : folders[(size_t) a])
            if (! hidden[(size_t) a].contains (f, true))
                visible.add (f);
        s.all[(size_t) a] = visible.size() == folders[(size_t) a].size();
        if (! s.all[(size_t) a])
            s.shown[(size_t) a] = visible;
    }
    return s;
}

AreaNames hiddenBy (const MenuFolderSlot& s, const AreaNames& folders)
{
    AreaNames hide;
    for (int a = 0; a < FolderTree::NumAreas; ++a)
        if (! s.all[(size_t) a])
            for (const auto& f : folders[(size_t) a])
                if (! s.shown[(size_t) a].contains (f, true))
                    hide[(size_t) a].add (f);
    return hide;
}
}
