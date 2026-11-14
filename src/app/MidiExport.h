#pragma once

#include "app/ExportPlan.h"

#include <juce_core/juce_core.h>

#include <functional>

// The export timeline as a Standard MIDI File (type 1: a conductor track with tempo and a time signature at every
// change of meter, a drum track on channel 10 with General MIDI notes). The hits come from the same PatternPlayer that
// plays the sound, so humanizing and flams stay; 1920 ticks per quarter keep the 1 ms raster down to 31 BPM. Clicks
// (count-in, "mit Klick") are the GM2 metronome notes 33 and 34 (bell on the downbeat). Hits that sound after the end
// of the music (laid back, humanized) are kept; both tracks end with the music (a rest at the end stays), or after the
// last note if that is later. shouldStop: asked every few milliseconds of music. Returns "" or what went wrong.
juce::String writeMidiExport (const ExportTimeline&, double tempo, const juce::String& title, const juce::File&,
                              const std::function<bool()>& shouldStop = {});

// The General MIDI drum note of a voice. GM has no brushes: brush hits and sweeps play the plain instrument.
int gmNoteFor (hz::Voice);
