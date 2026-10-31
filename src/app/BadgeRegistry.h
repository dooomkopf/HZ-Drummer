#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <string>
#include <vector>

namespace hz { struct Pattern; }

// Gives every pattern id a persistent badge (colour index, mark index): the first
// time an id shows up it gets a free badge by the rule below, and the choice is written to a file
// (~/.config/HZ-Drummer/abzeichen.json). Two patterns share a badge only once all badges are taken.
// When the palette grows, shared assignments move to free pairs; unique ones stay unchanged.
// Without a registry (tests, tools) the plain hash decides, see Song.cpp.
class BadgeRegistry
{
public:
    struct Badge { int colour = 0; int shape = 0; };

    // Which kind of mark a pattern gets (theme::isOwnMark: the geometric marks, else Material Symbols)
    enum class Rule
    {
        OwnFirst,   // the smallest folders get the own marks, as long as a whole folder still fits; all others symbols
        Mixed       // spread by the id's hash, the own marks weighted to fill up evenly across all folders
    };

    struct Stats
    {
        int patterns = 0;    // ids looked at
        int used = 0;        // distinct badges among them
        int shared = 0;      // ids that share their badge with another one
        int total = 0;       // badges available
    };

    void load (const juce::File&);                            // remembers the file; a missing file is an empty registry
    bool ensure (const std::vector<hz::Pattern>& library);   // assigns badges to new ids and saves; true if anything changed
    // forgets every assignment and gives all by the rule, rolled anew with this seed (0: the plain hash); saves
    void redistribute (const std::vector<hz::Pattern>& library, std::uint32_t seed);
    void release (const std::string& id);                    // forgets a deleted id and saves: its badge is free again
    Badge badgeFor (const std::string& id) const;            // registered badge, else the hash
    Stats stats (const std::vector<hz::Pattern>& library) const;

    Rule rule() const                             { return currentRule; }
    bool setRule (Rule);                                      // saves; applies to new ids and to redistribute()

    static Badge hashed (const std::string& id);             // the badge the id's hash points to
    static int totalBadges();
    static int ownBadges();                                  // colours x geometric marks

    static BadgeRegistry* global()                { return instance; }
    static void setGlobal (BadgeRegistry* r)      { instance = r; }

private:
    bool save() const;
    bool assign (const std::vector<hz::Pattern>& library, std::uint32_t seed);   // ensure() with a salted hash

    juce::File file;
    std::map<std::string, Badge> badges;
    Rule currentRule = Rule::OwnFirst;
    static BadgeRegistry* instance;
};
