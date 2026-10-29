#include "app/BadgeRegistry.h"

#include "app/views/SourceBadge.h"
#include "engine/Pattern.h"

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
}

int BadgeRegistry::totalBadges()
{
    return theme::numPartColours * theme::numBadgeShapes;
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
    if (! file.existsAsFile())
        return;
    const auto json = juce::JSON::parse (file);
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
    std::set<int> taken;
    for (auto& [id, b] : badges)
        taken.insert (linear (b));

    bool changed = false;
    for (auto& p : library)
    {
        if (! isPattern (p.id) || badges.count (p.id) > 0)
            continue;
        // start where the hash points, then probe forward for a free badge
        const int start = linear (hashed (p.id));
        int chosen = start;
        for (int k = 0; k < totalBadges(); ++k)
        {
            const int n = (start + k) % totalBadges();
            if (taken.count (n) == 0)
            {
                chosen = n;
                break;
            }
        }
        badges[p.id] = fromLinear (chosen);
        taken.insert (chosen);
        changed = true;
    }
    if (changed)
        save();
    return changed;
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
    auto* map = new juce::DynamicObject();
    for (auto& [id, b] : badges)
        map->setProperty (juce::String (id), juce::Array<juce::var> { b.colour, b.shape });
    root->setProperty ("badges", juce::var (map));

    juce::TemporaryFile temp (file);
    return temp.getFile().replaceWithText (juce::JSON::toString (juce::var (root))) && temp.overwriteTargetFileWithTemporary();
}
