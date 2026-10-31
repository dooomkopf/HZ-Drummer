#include "app/BadgeRegistry.h"

#include "app/views/SourceBadge.h"
#include "engine/Pattern.h"

#include <algorithm>
#include <set>

BadgeRegistry* BadgeRegistry::instance = nullptr;

namespace
{
    // FNV-1a over the id: the same id gives the same start badge on every machine
    std::uint32_t hashOf (const std::string& id)
    {
        std::uint32_t h = 2166136261u;
        for (unsigned char c : id)
            h = (h ^ c) * 16777619u;
        return h;
    }

    int linear (BadgeRegistry::Badge b)   { return b.colour * theme::numBadgeShapes + b.shape; }
    BadgeRegistry::Badge fromLinear (int n) { return { n / theme::numBadgeShapes, n % theme::numBadgeShapes }; }

    bool isPattern (const std::string& id) { return id.rfind ("song.", 0) != 0; }

    // the folder a pattern counts in for "own marks first": the bundled library, its band, or own
    std::string groupOf (const hz::Pattern& p)
    {
        if (p.id.rfind ("lib.", 0) == 0)
            return "lib.";
        for (const auto& tag : p.tags)
        {
            const juce::String value (tag);
            if (value.startsWithIgnoreCase ("band:") && value.substring (5).trim().isNotEmpty())
                return "band:" + value.substring (5).trim().toStdString();
        }
        return "own";
    }

    // the badges of one kind (geometric marks or Material Symbols), numbered colour by colour
    struct Kind
    {
        int first = 0, count = 0;   // shapes first .. first + count - 1
        int size() const { return theme::numPartColours * count; }
        BadgeRegistry::Badge at (int n) const { return { n / count, first + n % count }; }
    };
    const Kind ownKind { 0, theme::numOwnMarks };
    const Kind symbolKind { theme::numOwnMarks, theme::numBadgeShapes - theme::numOwnMarks };

    // a free badge of this kind: from where the hash points, the next free one
    int freeIn (const Kind& kind, std::uint32_t h, const std::set<int>& taken)
    {
        if (kind.size() == 0)
            return -1;
        const int start = (int) (h % (std::uint32_t) kind.size());
        for (int k = 0; k < kind.size(); ++k)
            if (const int n = linear (kind.at ((start + k) % kind.size())); taken.count (n) == 0)
                return n;
        return -1;
    }

    // what the rule wants for each pattern of this library
    struct Policy
    {
        BadgeRegistry::Rule rule = BadgeRegistry::Rule::OwnFirst;
        std::set<std::string> ownGroups;   // OwnFirst: the folders that get the own marks
        double ownShare = 0.0;             // Mixed: the share of patterns that gets one

        Policy (BadgeRegistry::Rule r, const std::vector<hz::Pattern>& library) : rule (r)
        {
            std::map<std::string, int> sizes;
            int patterns = 0;
            for (auto& p : library)
                if (isPattern (p.id))
                {
                    ++sizes[groupOf (p)];
                    ++patterns;
                }
            ownShare = patterns > 0 ? juce::jmin (1.0, (double) BadgeRegistry::ownBadges() / patterns) : 0.0;

            // smallest folders first, as long as a whole folder fits into the own marks
            std::vector<std::pair<int, std::string>> bySize;
            for (auto& [group, n] : sizes)
                bySize.push_back ({ n, group });
            std::sort (bySize.begin(), bySize.end());
            int used = 0;
            for (auto& [n, group] : bySize)
            {
                if (used + n > BadgeRegistry::ownBadges())
                    break;
                used += n;
                ownGroups.insert (group);
            }
        }

        bool wantsOwn (const hz::Pattern& p, const std::string& salt) const
        {
            if (rule == BadgeRegistry::Rule::OwnFirst)
                return ownGroups.count (groupOf (p)) > 0;
            return (double) hashOf ("mix:" + salt + p.id) / 4294967296.0 < ownShare;   // a second hash: independent of the badge
        }
    };

    const char* ruleName (BadgeRegistry::Rule r) { return r == BadgeRegistry::Rule::Mixed ? "gemischt" : "eigene-zuerst"; }
}

int BadgeRegistry::totalBadges()
{
    return theme::numPartColours * theme::numBadgeShapes;
}

int BadgeRegistry::ownBadges()
{
    return theme::numPartColours * theme::numOwnMarks;
}

BadgeRegistry::Badge BadgeRegistry::hashed (const std::string& id)
{
    const auto h = hashOf (id);
    return { (int) (h % (std::uint32_t) theme::numPartColours),
             (int) ((h / (std::uint32_t) theme::numPartColours) % (std::uint32_t) theme::numBadgeShapes) };
}

void BadgeRegistry::load (const juce::File& f)
{
    file = f;
    badges.clear();
    currentRule = Rule::OwnFirst;
    if (! file.existsAsFile())
        return;
    const auto json = juce::JSON::parse (file);
    if (json["regel"].toString() == ruleName (Rule::Mixed))
        currentRule = Rule::Mixed;
    if (auto* obj = json["badges"].getDynamicObject())
        for (auto& prop : obj->getProperties())
            if (auto* pair = prop.value.getArray(); pair != nullptr && pair->size() == 2)
            {
                const int c = (int) (*pair)[0], s = (int) (*pair)[1];
                if (c >= 0 && c < theme::numPartColours && s >= 0 && s < theme::numBadgeShapes)
                    badges[prop.name.toString().toStdString()] = { c, s };
            }
}

bool BadgeRegistry::ensure (const std::vector<hz::Pattern>& library)
{
    return assign (library, 0);
}

bool BadgeRegistry::assign (const std::vector<hz::Pattern>& library, std::uint32_t seed)
{
    const std::string salt = seed != 0 ? std::to_string (seed) + ":" : std::string();   // 0: the plain hash, as before
    juce::Logger::writeToLog ("Abzeichen-Zuweisung: " + juce::String ((int) library.size())
        + " Eintraege, " + juce::String (totalBadges()) + " Kombinationen");
    std::set<int> taken;
    for (auto& [id, b] : badges)
        taken.insert (linear (b));

    const Policy policy (currentRule, library);
    bool changed = false;
    std::set<int> seenInLibrary;
    for (auto& p : library)
    {
        if (! isPattern (p.id))
            continue;
        const auto existing = badges.find (p.id);
        if (existing != badges.end() && seenInLibrary.insert (linear (existing->second)).second)
            continue;   // a badge of its own never changes
        // new, or sharing a badge from a time the palette was full: a free one of the kind the rule wants,
        // else of the other kind
        const auto h = hashOf (salt + p.id);
        const bool own = policy.wantsOwn (p, salt);
        int chosen = freeIn (own ? ownKind : symbolKind, h, taken);
        if (chosen < 0)
            chosen = freeIn (own ? symbolKind : ownKind, h, taken);
        if (chosen < 0)
        {
            if (existing != badges.end())
                continue;   // all taken: keep sharing rather than churn on every reload
            chosen = linear (hashed (p.id));
        }
        badges[p.id] = fromLinear (chosen);
        taken.insert (chosen);
        seenInLibrary.insert (chosen);
        changed = true;
    }
    if (changed)
    {
        const bool saved = save();
        juce::Logger::writeToLog ("Abzeichen: " + juce::String ((int) library.size()) + " Eintraege, "
            + juce::String (totalBadges()) + " Kombinationen, gespeichert: " + (saved ? "ja" : "nein")
            + " (" + file.getFullPathName() + ")");
    }
    return changed;
}

void BadgeRegistry::redistribute (const std::vector<hz::Pattern>& library, std::uint32_t seed)
{
    badges.clear();   // also forgets ids that are no longer in the library
    assign (library, seed);   // now every pattern is new: all by the rule, in library order
    save();
}

bool BadgeRegistry::setRule (Rule r)
{
    currentRule = r;
    return save();
}

void BadgeRegistry::release (const std::string& id)
{
    if (badges.erase (id) > 0)
        save();
}

BadgeRegistry::Badge BadgeRegistry::badgeFor (const std::string& id) const
{
    auto it = badges.find (id);
    return it != badges.end() ? it->second : hashed (id);
}

BadgeRegistry::Stats BadgeRegistry::stats (const std::vector<hz::Pattern>& library) const
{
    Stats st;
    st.total = totalBadges();
    std::map<int, int> count;
    for (auto& p : library)
        if (isPattern (p.id))
        {
            ++st.patterns;
            ++count[linear (badgeFor (p.id))];
        }
    st.used = (int) count.size();
    for (auto& [n, c] : count)
        if (c > 1)
            st.shared += c;
    return st;
}

bool BadgeRegistry::save() const
{
    if (file == juce::File())
        return false;
    auto* root = new juce::DynamicObject();
    root->setProperty ("formatVersion", 1);
    root->setProperty ("regel", ruleName (currentRule));
    auto* map = new juce::DynamicObject();
    for (auto& [id, b] : badges)
        map->setProperty (juce::String (id), juce::Array<juce::var> { b.colour, b.shape });
    root->setProperty ("badges", juce::var (map));

    juce::TemporaryFile temp (file, juce::TemporaryFile::useHiddenFile);   // hidden: a leftover after a crash is never loaded
    return temp.getFile().replaceWithText (juce::JSON::toString (juce::var (root))) && temp.overwriteTargetFileWithTemporary();
}
