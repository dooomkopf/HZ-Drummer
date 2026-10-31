#include "app/TrainerSlots.h"

namespace trainerslots
{
juce::File file()
{
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".config/HZ-Drummer/trainer.json");
}

Slots load (const juce::File& f)
{
    Slots slots;
    const auto root = juce::JSON::parse (f);
    const auto* list = root["plaetze"].getArray();
    if (list == nullptr)
        return slots;
    for (int i = 0; i < juce::jmin (count, list->size()); ++i)
    {
        const auto& v = list->getReference (i);
        auto& s = slots[(size_t) i];
        s.name = v["name"].toString().trim();
        if (s.isFree())
            continue;
        s.patternId = v["baustein"].toString().toStdString();
        s.meterKey = v["taktart"].toString();
        s.loopFirst = v.hasProperty ("loopVon") ? (int) v["loopVon"] : -1;
        s.loopLast = v.hasProperty ("loopBis") ? (int) v["loopBis"] : -1;
        s.loopOn = (bool) v["loopAn"];
        if (s.loopFirst < 0 || s.loopLast < s.loopFirst)
        {
            s.loopFirst = s.loopLast = -1;
            s.loopOn = false;
        }
        s.to = juce::jlimit (31.0, 320.0, (double) v.getProperty ("bis", 140.0));
        s.from = juce::jlimit (30.0, s.to - 1.0, (double) v.getProperty ("von", 100.0));
        s.step = juce::jlimit (1.0, 40.0, (double) v.getProperty ("inkrement", 1.0));
        s.repeats = juce::jlimit (1, 5, (int) v.getProperty ("wiederholungen", 1));
    }
    return slots;
}

bool save (const Slots& slots, const juce::File& f)
{
    juce::Array<juce::var> list;
    for (const auto& s : slots)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", s.name);
        if (! s.isFree())
        {
            o->setProperty ("baustein", juce::String (s.patternId));
            o->setProperty ("taktart", s.meterKey);
            o->setProperty ("loopVon", s.loopFirst);
            o->setProperty ("loopBis", s.loopLast);
            o->setProperty ("loopAn", s.loopOn);
            o->setProperty ("von", s.from);
            o->setProperty ("bis", s.to);
            o->setProperty ("inkrement", s.step);
            o->setProperty ("wiederholungen", s.repeats);
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
        // written and flushed by hand: File::replaceWithText ignores a failed write and would put an empty or
        // half file in place of the slots when the disk is full
        juce::FileOutputStream out (temp.getFile());
        if (! out.openedOk() || ! out.writeText (juce::JSON::toString (juce::var (root)), false, false, "\n"))
            return false;
        out.flush();
        if (out.getStatus().failed())
            return false;
    }
    return temp.overwriteTargetFileWithTemporary();
}
}
