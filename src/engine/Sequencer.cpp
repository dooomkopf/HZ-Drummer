#include "engine/Sequencer.h"

#include <algorithm>

namespace hz
{

void Sequencer::prepare (double newSampleRate)
{
    newSampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    // convert the remaining time into the new rate, otherwise the next pulse is off
    samplesToNextPulse *= newSampleRate / sampleRate;
    sampleRate = newSampleRate;
}

void Sequencer::setTempo (double quarterBpm)
{
    tempo = std::clamp (quarterBpm, minTempo, maxTempo);
}

void Sequencer::setMeter (const Meter& newMeter, int tag)
{
    if (! newMeter.isValid())
        return;

    pendingMeter = newMeter;
    pendingTag = tag;
    hasPending = true;

    if (! playing)
        applyPendingMeter();
}

void Sequencer::start()
{
    applyPendingMeter();
    playing = true;
    samplesToNextPulse = 0.0;
    bar = 0;
    pulse = 0;
}

void Sequencer::stop()
{
    playing = false;
}

int Sequencer::process (int numSamples, ClickEvent* out, int maxEvents)
{
    if (! playing)
        return 0;

    int count = 0;
    double pos = samplesToNextPulse;

    // pos is carried on as double: no drift from rounding over many bars
    while (pos < (double) numSamples)
    {
        // Take over a meter change only right before the next downbeat: this way even
        // a request arriving during the last pulse of the bar still takes effect in time
        if (pulse == 0)
            applyPendingMeter();

        if (count < maxEvents)
            out[count++] = { (int) pos, meter.accentAt (pulse), bar, pulse, meterTag };

        pos += samplesPerPulse();

        if (++pulse >= meter.pulsesPerBar())
        {
            pulse = 0;
            ++bar;
        }
    }

    samplesToNextPulse = pos - (double) numSamples;
    return count;
}

double Sequencer::samplesPerPulse() const
{
    return sampleRate * 60.0 / tempo * meter.quartersPerPulse();
}

void Sequencer::applyPendingMeter()
{
    if (! hasPending)
        return;

    meter = pendingMeter;
    meterTag = pendingTag;
    hasPending = false;
}

} // namespace hz
