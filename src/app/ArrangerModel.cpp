#include "app/ArrangerModel.h"
#include "app/SongRename.h"

#include <algorithm>

juce::String ArrangerModel::contentKey (const Song& s)
{
    // identity is not content: neither the file id nor the block uids (older files get random ones on load)
    Song copy = s;
    copy.id.clear();
    for (auto& p : copy.parts)
        p.uid.clear();
    return juce::JSON::toString (songToJson (copy), true);
}

bool ArrangerModel::hasUnsavedChanges() const
{
    return contentKey (current.song) != savedKey;
}

void ArrangerModel::markSavedAs (const Song& saved)
{
    savedKey = contentKey (saved);
}

void ArrangerModel::markUnsaved()
{
    savedKey = "\x01";   // matches nothing
}

void ArrangerModel::load (Song s)
{
    current.song = std::move (s);
    savedKey = contentKey (current.song);
    undoStack.clear();
    redoStack.clear();
    if (onChange)
        onChange();
}

void ArrangerModel::adoptSaved (const Song& saved)
{
    // the file identity belongs to every snapshot, otherwise undo would resurrect the old id
    for (auto* stack : { &undoStack, &redoStack })
        for (auto& st : *stack)
            st.song.id = saved.id;
    current.song = saved;
    savedKey = contentKey (saved);
    if (onChange)
        onChange();
}

void ArrangerModel::loadScratch (std::vector<SongPart> list)
{
    current.scratch = std::move (list);
    if (onChange)
        onChange();
}

bool ArrangerModel::commit (State next, bool scratchChanged)
{
    // Removing parts must remain possible even when restoring an old over-limit song.
    if (! scratchChanged && ! next.song.parts.empty()
        && next.song.parts.size() >= current.song.parts.size() && validateSong)
    {
        const auto error = validateSong (next.song);
        if (error.isNotEmpty())
        {
            if (onRejected)
                onRejected (error);
            return false;
        }
    }
    undoStack.push_back (std::move (current));
    if ((int) undoStack.size() > maxUndo)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
    current = std::move (next);

    if (scratchChanged && onScratchChange)
        onScratchChange();
    if (onChange)
        onChange();
    return true;
}

void ArrangerModel::restore (State s)
{
    const bool scratchChanged = s.scratch.size() != current.scratch.size()
        || ! std::equal (s.scratch.begin(), s.scratch.end(), current.scratch.begin(), sameBlock);
    current = std::move (s);
    if (scratchChanged && onScratchChange)
        onScratchChange();
    if (onChange)
        onChange();
}

void ArrangerModel::undo()
{
    if (undoStack.empty())
        return;
    auto s = std::move (undoStack.back());
    undoStack.pop_back();
    redoStack.push_back (current);
    restore (std::move (s));
}

void ArrangerModel::redo()
{
    if (redoStack.empty())
        return;
    auto s = std::move (redoStack.back());
    redoStack.pop_back();
    undoStack.push_back (current);
    restore (std::move (s));
}

//==============================================================================
void ArrangerModel::insertPart (int index, SongPart part)
{
    insertParts (index, { std::move (part) });
}

void ArrangerModel::insertParts (int index, std::vector<SongPart> parts)
{
    if (parts.empty())
        return;
    auto next = current;
    auto& seq = next.song.parts;
    index = std::clamp (index, 0, (int) seq.size());
    // every block in a song has its own identity: a duplicate or a re-used Ablage entry gets a new one
    for (auto& p : parts)
    {
        const bool taken = std::any_of (seq.begin(), seq.end(), [&] (const SongPart& q) { return q.uid == p.uid; });
        if (p.uid.empty() || taken)
            p.uid = newBlockUid();
    }
    seq.insert (seq.begin() + index, std::make_move_iterator (parts.begin()), std::make_move_iterator (parts.end()));
    commit (std::move (next), false);
}

void ArrangerModel::removePart (int index)
{
    if (! validPart (index))
        return;
    auto next = current;
    next.song.parts.erase (next.song.parts.begin() + index);
    commit (std::move (next), false);
}

void ArrangerModel::movePart (int from, int to)
{
    if (! validPart (from))
        return;
    to = std::clamp (to, 0, (int) current.song.parts.size() - 1);
    if (to == from)
        return;
    auto next = current;
    auto& seq = next.song.parts;
    auto part = std::move (seq[(size_t) from]);
    seq.erase (seq.begin() + from);
    seq.insert (seq.begin() + to, std::move (part));
    commit (std::move (next), false);
}

void ArrangerModel::setRepeats (int index, int repeats)
{
    repeats = std::clamp (repeats, 1, 64);
    if (! validPart (index) || current.song.parts[(size_t) index].repeats == repeats)
        return;
    auto next = current;
    next.song.parts[(size_t) index].repeats = repeats;
    commit (std::move (next), false);
}

void ArrangerModel::setRange (int index, int firstBar, int numBars)
{
    if (! validPart (index))
        return;
    const auto& part = current.song.parts[(size_t) index];
    if (part.kind != PartKind::Pattern || ! part.copy)
        return;
    const int available = (int) part.copy->bars.size();
    firstBar = std::clamp (firstBar, 0, available - 1);
    numBars = std::clamp (numBars, 0, available - firstBar);
    if (numBars == available || numBars == 0)
        firstBar = numBars = 0;   // the whole pattern
    if (part.firstBar == firstBar && part.numBars == numBars)
        return;
    auto next = current;
    next.song.parts[(size_t) index].firstBar = firstBar;
    next.song.parts[(size_t) index].numBars = numBars;
    commit (std::move (next), false);
}

int ArrangerModel::indexOfUid (const std::string& uid) const
{
    for (int i = 0; i < (int) current.song.parts.size(); ++i)
        if (current.song.parts[(size_t) i].uid == uid)
            return i;
    return -1;
}

void ArrangerModel::setLabel (int index, const std::string& label)
{
    if (! validPart (index) || current.song.parts[(size_t) index].label == label)
        return;
    auto next = current;
    next.song.parts[(size_t) index].label = label;
    commit (std::move (next), false);
}

bool ArrangerModel::setCopy (int index, const hz::Pattern& copy, bool edited)
{
    if (! validPart (index) || current.song.parts[(size_t) index].kind != PartKind::Pattern)
        return false;
    auto next = current;
    auto& part = next.song.parts[(size_t) index];
    part.copy = std::make_shared<const hz::Pattern> (copy);
    part.edited = edited;
    // a bar range that no longer fits falls back to the whole pattern
    if (part.numBars > 0 && part.firstBar + part.numBars > (int) copy.bars.size())
        part.firstBar = part.numBars = 0;
    return commit (std::move (next), false);
}

int ArrangerModel::updateFromSource (const std::vector<int>& indices, const std::vector<hz::Pattern>& library)
{
    auto next = current;
    int changed = 0;
    for (int index : indices)
    {
        if (! validPart (index))
            continue;
        auto& part = next.song.parts[(size_t) index];
        for (auto& p : library)
            if (p.id == part.sourceId && part.kind == PartKind::Pattern)
            {
                part.copy = std::make_shared<const hz::Pattern> (p);
                part.edited = false;
                if (part.numBars > 0 && part.firstBar + part.numBars > (int) p.bars.size())
                    part.firstBar = part.numBars = 0;
                ++changed;
                break;
            }
    }
    if (changed > 0 && ! commit (std::move (next), false))
        return 0;
    return changed;
}

std::vector<int> ArrangerModel::partsWithSource (const std::string& sourceId) const
{
    std::vector<int> out;
    for (int i = 0; i < (int) current.song.parts.size(); ++i)
        if (current.song.parts[(size_t) i].kind == PartKind::Pattern && current.song.parts[(size_t) i].sourceId == sourceId)
            out.push_back (i);
    return out;
}

void ArrangerModel::setName (const std::string& name)
{
    if (name.empty() || current.song.name == name)
        return;
    auto next = current;
    next.song.name = name;
    commit (std::move (next), false);
}

int ArrangerModel::renameCopies (const std::string& sourceId, const std::string& oldName, const std::string& newName)
{
    for (auto* stack : { &undoStack, &redoStack })
        for (auto& state : *stack)
        {
            ::renameCopies (state.song.parts, sourceId, oldName, newName);
            ::renameCopies (state.scratch, sourceId, oldName, newName);
        }
    const int inSong = ::renameCopies (current.song.parts, sourceId, oldName, newName);
    const int inScratch = ::renameCopies (current.scratch, sourceId, oldName, newName);
    if (inScratch > 0 && onScratchChange)
        onScratchChange();
    if (inSong + inScratch > 0 && onChange)
        onChange();
    return inSong + inScratch;
}

void ArrangerModel::setTempo (double bpm)
{
    if (current.song.tempo == bpm)
        return;
    auto next = current;
    next.song.tempo = bpm;
    commit (std::move (next), false);
}

//==============================================================================
int ArrangerModel::addToScratch (std::vector<SongPart> parts)
{
    auto next = current;
    int added = 0;
    for (auto& p : parts)
    {
        const bool known = std::any_of (next.scratch.begin(), next.scratch.end(), [&] (const SongPart& s) { return sameBlock (s, p); });
        if (! known)
        {
            next.scratch.push_back (std::move (p));
            ++added;
        }
    }
    if (added > 0)
        commit (std::move (next), true);
    return added;
}

void ArrangerModel::moveToScratch (int index)
{
    if (! validPart (index))
        return;
    auto next = current;
    auto part = next.song.parts[(size_t) index];
    next.song.parts.erase (next.song.parts.begin() + index);
    const bool known = std::any_of (next.scratch.begin(), next.scratch.end(), [&] (const SongPart& s) { return sameBlock (s, part); });
    if (! known)
        next.scratch.push_back (std::move (part));
    commit (std::move (next), ! known);
}

int ArrangerModel::indexOfScratchUid (const std::string& uid) const
{
    for (int i = 0; i < (int) current.scratch.size(); ++i)
        if (current.scratch[(size_t) i].uid == uid)
            return i;
    return -1;
}

bool ArrangerModel::setScratchCopy (int index, const hz::Pattern& copy, bool edited)
{
    if (index < 0 || index >= (int) current.scratch.size() || current.scratch[(size_t) index].kind != PartKind::Pattern)
        return false;
    auto next = current;
    auto& part = next.scratch[(size_t) index];
    part.copy = std::make_shared<const hz::Pattern> (copy);
    part.edited = edited;
    if (part.numBars > 0 && part.firstBar + part.numBars > (int) copy.bars.size())
        part.firstBar = part.numBars = 0;
    return commit (std::move (next), true);
}

bool ArrangerModel::insertIntoScratch (int index, SongPart part)
{
    if (std::any_of (current.scratch.begin(), current.scratch.end(), [&] (const SongPart& s) { return sameBlock (s, part); }))
        return false;
    auto next = current;
    index = std::clamp (index, 0, (int) next.scratch.size());
    next.scratch.insert (next.scratch.begin() + index, std::move (part));
    return commit (std::move (next), true);
}

void ArrangerModel::removeFromScratch (int index)
{
    if (index < 0 || index >= (int) current.scratch.size())
        return;
    auto next = current;
    next.scratch.erase (next.scratch.begin() + index);
    commit (std::move (next), true);
}

void ArrangerModel::clearScratch()
{
    if (current.scratch.empty())
        return;
    auto next = current;
    next.scratch.clear();
    commit (std::move (next), true);
}

//==============================================================================
SongPart makePatternPart (const hz::Pattern& p)
{
    SongPart part;
    part.uid = newBlockUid();
    part.kind = PartKind::Pattern;
    part.sourceId = p.id;
    part.copy = std::make_shared<const hz::Pattern> (p);
    return part;
}
