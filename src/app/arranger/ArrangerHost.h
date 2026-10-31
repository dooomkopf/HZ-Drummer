#pragma once

#include "app/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>

class Library;
struct PlaybackView;

// What the arranger needs from the main window. Auditioning (one block in a loop) and playing
// the arranged sequence both go through the main window's transport, so Start/Stop, tempo and
// the song strip there always show what is audible.
class ArrangerHost
{
public:
    virtual ~ArrangerHost() = default;

    virtual const Library& getLibrary() const = 0;

    virtual void arrangerAudition (const SongPart&) = 0;         // loop this block, start if stopped
    virtual void arrangerPlay (const Song&, int fromPart) = 0;   // play the sequence from a part
    virtual void arrangerUpdate (const Song&) = 0;               // the sequence changed while it plays: keep position
    virtual void arrangerSeekToPart (int part) = 0;              // jump to the start of a part (playing or paused)
    virtual void arrangerSeekToBar (int bar) = 0;                // jump to a bar of the sequence (playing or paused)
    virtual void arrangerStop() = 0;                             // stop, back to the main selection
    virtual bool arrangerAuditionActive() const = 0;             // an audition is sounding right now
    virtual bool arrangerPlayActive() const = 0;                 // the sequence is playing
    virtual bool arrangerPlanLoaded() const = 0;                 // the sequence is what the transport holds (playing or paused)
    virtual std::string currentPlanId() const = 0;               // id of the plan shown ("song.<id>"), playing or paused
    virtual const PlaybackView& getPlaybackView() const = 0;     // position while the sequence plays

    virtual juce::String arrangerSaveSong (Song&) = 0;           // "" = OK, else an error text
    virtual bool handleTransportKey (const juce::KeyPress&) = 0; // space, tempo, tap
};
