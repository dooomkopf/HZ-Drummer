#pragma once

#include "engine/Pattern.h"

namespace hz
{

// Editing operations for the grid editor. All keep the pattern valid (validate() == "")
// and preserve the IDs of existing events; new events get fresh IDs.

// Role (hands/feet) a hit of this voice belongs to
LayerRole roleFor (Voice);

// Find the event at exactly this position and voice; nullptr if none
const PatternEvent* findEvent (const Pattern&, Voice, Rational pos);

// Sets a hit (velocity) or removes it if one is already there. Returns true = set.
bool toggleEvent (Pattern&, Voice, Rational pos, float velocity);

// Set the velocity of an existing hit (0.05..1); false if there is none
bool setEventVelocity (Pattern&, Voice, Rational pos, float velocity);
bool setEventLag (Pattern&, Voice, Rational pos, float lagMs);   // -100..100 ms: later (+) or earlier (-) than the position

// Remove a hit; false if there was none
bool removeEvent (Pattern&, Voice, Rational pos);

// Insert a bar after 'afterBar' (same meter/grid as that one); following hits shift back.
// With copyContent the bar's hits are copied too.
void insertBar (Pattern&, int afterBar, bool copyContent);

// Remove a bar (not the last remaining one); its hits are dropped, following hits shift forward
bool removeBar (Pattern&, int bar);

// Change a bar's meter. Hits that no longer fit are dropped; following bars shift.
void setBarMeter (Pattern&, int bar, const Meter&);

// Grid resolution of a bar (view only; existing hits stay where they are)
void setBarGrid (Pattern&, int bar, Rational grid);

// Remove all hits of a voice in one bar
void clearLane (Pattern&, int bar, Voice);

// Bars [first, first + count) as a pattern of their own: hits shifted to start at 0, ids kept.
// The range is clamped to the pattern; an empty range gives the first bar.
Pattern sliceBars (const Pattern&, int first, int count);

} // namespace hz
