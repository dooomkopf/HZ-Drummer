#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <string>
#include <vector>

namespace hz { struct Pattern; }

// Gives every pattern id a badge (colour index, mark index) that then never changes: the first
// time an id shows up it gets the badge its hash points to, or the next free one in a fixed
// probing order, and the choice is written to a file (~/.config/HZ-Drummer/abzeichen.json).
// Two patterns share a badge only once all badges are taken. Without a registry (tests, tools)
// the plain hash decides, see Song.cpp.
class BadgeRegistry
{
public:
    struct Badge { int colour = 0; int shape = 0; };

    struct Stats
    {
        int patterns = 0;    // ids looked at
        int used = 0;        // distinct badges among them
        int shared = 0;      // ids that share their badge with another one
        int total = 0;       // badges available
    };

    void load (const juce::File&);                            // remembers the file; a missing file is an empty registry
    bool ensure (const std::vector<hz::Pattern>& library);   // assigns badges to new ids and saves; true if anything changed
    void release (const std::string& id);                    // forgets a deleted id and saves: its badge is free again
    Badge badgeFor (const std::string& id) const;            // registered badge, else the hash
    Stats stats (const std::vector<hz::Pattern>& library) const;

    static Badge hashed (const std::string& id);             // the badge the id's hash points to
    static int totalBadges();

    static BadgeRegistry* global()                { return instance; }
    static void setGlobal (BadgeRegistry* r)      { instance = r; }

private:
    bool save() const;

    juce::File file;
    std::map<std::string, Badge> badges;
    static BadgeRegistry* instance;
};
