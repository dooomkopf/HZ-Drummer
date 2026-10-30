#include "app/AudioEngine.h"
#include "app/MixState.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int maxChunk = 256;              // bounds the number of events per player call
    constexpr double kickAlternateSec = 0.22;  // faster kick sequences alternate feet
}

//==============================================================================
std::uint64_t AudioEngine::Control::pack() const
{
    return (std::uint64_t) tempoTenths
         | ((std::uint64_t) (click ? 1 : 0) << 16)
         | ((std::uint64_t) (play ? 1 : 0) << 17)
         | ((std::uint64_t) generation << 32);
}

AudioEngine::Control AudioEngine::Control::unpack (std::uint64_t v)
{
    Control c;
    c.tempoTenths = (std::uint16_t) (v & 0xffff);
    c.click       = ((v >> 16) & 1) != 0;
    c.play        = ((v >> 17) & 1) != 0;
    c.generation  = (std::uint16_t) ((v >> 32) & 0xffff);
    return c;
}

//==============================================================================
// UI thread

void AudioEngine::sendPlan (std::unique_ptr<hz::PatternPlan> plan)
{
    unsentPlan.reset();   // a newer plan always replaces an older one that was not sent yet

    const auto scope = planFifo.write (1);
    if (scope.blockSize1 == 0)
    {
        unsentPlan = std::move (plan);   // queue full: retry from collectRetiredPlans()
        return;
    }

    planQueue[(size_t) scope.startIndex1] = plan.get();
    ownedPlans.push_back (std::move (plan));
}

void AudioEngine::collectRetiredPlans()
{
    const auto scope = retireFifo.read (retireFifo.getNumReady());
    auto release = [this] (const hz::PatternPlan* p)
    {
        ownedPlans.erase (std::remove_if (ownedPlans.begin(), ownedPlans.end(), [p] (const auto& o) { return o.get() == p; }),
                          ownedPlans.end());
    };
    for (int i = 0; i < scope.blockSize1; ++i) release (retireQueue[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) release (retireQueue[(size_t) (scope.startIndex2 + i)]);

    if (unsentPlan != nullptr)
        sendPlan (std::move (unsentPlan));
}

void AudioEngine::popDisplayEvents (std::vector<DisplayEvent>& out)
{
    const auto scope = displayFifo.read (displayFifo.getNumReady());
    for (int i = 0; i < scope.blockSize1; ++i) out.push_back (displayBuffer[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) out.push_back (displayBuffer[(size_t) (scope.startIndex2 + i)]);
}

//==============================================================================
// Audio thread

void AudioEngine::prepare (double newSampleRate, int outputLatencySamples)
{
    sampleRate = newSampleRate;
    latencySamples = outputLatencySamples;
    player.prepare (newSampleRate);
    click.prepare (newSampleRate);
    guard.prepare (newSampleRate);
    sampler.prepare (newSampleRate);
    deviceRate = newSampleRate;
}

void AudioEngine::triggerVoice (const hz::PlayEvent& e, std::int64_t blockStart)
{
    bool left = e.limb == hz::Limb::Left;

    if (e.voice.instrument == hz::Instrument::Kick && e.voice.articulation == hz::Articulation::Normal)
    {
        // automatic foot choice: alternate on fast sequences, otherwise right foot
        const std::int64_t at = blockStart + e.sampleOffset;
        if (e.limb == hz::Limb::Auto)
            left = (at - lastKickSample) < (std::int64_t) (kickAlternateSec * sampleRate) && ! lastKickLeft;
        lastKickSample = at;
        lastKickLeft = left;
    }

    if (const int inst = sampler.getKit()->instrumentFor (e.voice, left); inst >= 0)
        sampler.trigger (inst, e.velocity, e.sampleOffset, mix::channelFor (e.voice));
}

void AudioEngine::process (float* left, float* right, int numSamples)
{
    // Take over a new kit. A kit with the wrong sample rate stays silent (it would be out of tune)
    // until KitManager has reloaded it at the device rate.
    if (auto* k = kitForAudio.load(); k != sampler.getKit())
        sampler.setKit (k);
    kitInUseByAudio = sampler.getKit();
    const bool samplesOk = sampler.getKit() != nullptr && std::abs (sampler.getKit()->sampleRate - sampleRate) < 0.5;

    // New plans: the newest wins, the player hands back skipped ones. Only take as many as can
    // safely be returned (minus one reserve slot for the switch at the loop end).
    {
        const int room = std::min (player.freeRetireSlots(), (int) retireBacklog.size() - retireBacklogSize) - 1;
        const auto scope = planFifo.read (juce::jlimit (0, planFifo.getNumReady(), room));
        for (int i = 0; i < scope.blockSize1; ++i) player.setNextPlan (planQueue[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) player.setNextPlan (planQueue[(size_t) (scope.startIndex2 + i)]);
    }

    const auto c = Control::unpack (control.load());
    player.setTempo (c.tempoTenths / 10.0);
    if (c.generation != appliedGeneration)
    {
        if (c.play) player.start(); else player.stop();
        appliedGeneration = c.generation;
    }
    if (const auto serial = seekSerial.load(); serial != appliedSeek)
    {
        player.seek (seekTarget.load());
        appliedSeek = serial;
    }
    if (const auto request = previewRequest.load(); (std::uint32_t) (request >> 32) != appliedPreview)
    {
        // a preview hit (lane name clicked): at the start of this block, whatever the transport does
        appliedPreview = (std::uint32_t) (request >> 32);
        const auto packed = (std::uint32_t) request;
        hz::PlayEvent hit;
        hit.voice = { (hz::Instrument) (packed & 0xffu), (hz::Articulation) ((packed >> 8) & 0xffu) };
        hit.velocity = (float) ((packed >> 16) & 0xffu) / 255.0f;
        if (sampler.getKit() != nullptr && std::abs (sampler.getKit()->sampleRate - sampleRate) < 0.5)
            triggerVoice (hit, audioClock);
    }

    // one consistent mixer snapshot per block
    const auto mixNow = mixState.load();
    const float master = mix::gainOf (mixNow, mix::Master);
    for (int ch = 0; ch < mix::Click; ++ch)
        sampler.setChannelGain (ch, mix::gainOf (mixNow, ch));
    // master and click follow the mixer smoothly (an abrupt jump would click)
    const float smooth = 1.0f - std::exp (-(float) numSamples / (0.020f * (float) sampleRate));
    masterNow += (master - masterNow) * smooth;
    clickNow += (mix::gainOf (mixNow, mix::Click) - clickNow) * smooth;
    sampler.setGain (0.8f * masterNow);
    click.setGain (0.5f * masterNow * clickNow);

    const double blockStartMs = juce::Time::getMillisecondCounterHiRes();
    sampler.resetPeaks();
    click.resetPeak();
    float masterPeak = 0.0f;

    for (int done = 0; done < numSamples; )
    {
        const int len = std::min (maxChunk, numSamples - done);
        float* l = left + done;
        float* r = right != nullptr ? right + done : nullptr;

        int numPlay = 0, numPulse = 0;
        player.process (len, playEvents.data(), (int) playEvents.size(), numPlay, pulseEvents.data(), (int) pulseEvents.size(), numPulse);

        // order by actual time (humanizing/flams may reorder) so that the foot alternation is right
        std::sort (playEvents.begin(), playEvents.begin() + numPlay,
                   [] (const hz::PlayEvent& a, const hz::PlayEvent& b) { return a.sampleOffset < b.sampleOffset; });
        // speaker protection: no plan needs more than a handful of hits per chunk; a burst
        // (as once happened when a whole song collapsed onto one bar end) is cut off here
        numPlay = std::min (numPlay, maxHitsPerChunk);
        if (samplesOk)
            for (int i = 0; i < numPlay; ++i)
                triggerVoice (playEvents[(size_t) i], audioClock + done);

        // metronome mode clicks every pulse; songs click only in their "Klick" bars
        int numClicks = 0;
        for (int i = 0; i < numPulse; ++i)
            if (c.click || pulseEvents[(size_t) i].click)
                clickEvents[(size_t) numClicks++] = { pulseEvents[(size_t) i].sampleOffset, pulseEvents[(size_t) i].accent, 0, 0, 0 };
        click.render (l, r, len, clickEvents.data(), numClicks);   // also lets a click ring out
        sampler.render (l, r, len);
        guard.process (l, r, len);   // speaker protection: limiter + NaN guard + hard clamp
        for (int i = 0; i < len; ++i)
            masterPeak = std::max (masterPeak, std::max (std::abs (l[i]), r != nullptr ? std::abs (r[i]) : 0.0f));

        for (int i = 0; i < numPulse; ++i)
        {
            const auto& e = pulseEvents[(size_t) i];
            const double t = blockStartMs + 1000.0 * (done + e.sampleOffset + latencySamples) / sampleRate;
            const auto scope = displayFifo.write (1);
            if (scope.blockSize1 > 0)
                displayBuffer[(size_t) scope.startIndex1] = { t, e.bar, e.pulse, e.loop, e.meterTag, e.numBars, e.planTag, c.generation };
        }

        done += len;
    }
    audioClock += numSamples;

    // level meters: the UI takes the highest value since it last looked
    auto publish = [this] (int ch, float level)
    {
        auto& slot = levels[(size_t) ch];
        for (float seen = slot.load(); level > seen && ! slot.compare_exchange_weak (seen, level);) {}
    };
    for (int ch = 0; ch < mix::Click; ++ch)
        publish (ch, sampler.peak (ch));
    publish (mix::Click, click.peak());
    publish (mix::Master, masterPeak);

    returnRetiredPlans();
}

void AudioEngine::returnRetiredPlans()
{
    // hand replaced plans back to the UI (if the FIFO is full, keep them for the next block)
    while (const auto* old = player.popRetired())
        if (retireBacklogSize < (int) retireBacklog.size())
            retireBacklog[(size_t) retireBacklogSize++] = old;

    while (retireBacklogSize > 0)
    {
        const auto scope = retireFifo.write (1);
        if (scope.blockSize1 == 0)
            break;
        retireQueue[(size_t) scope.startIndex1] = retireBacklog[(size_t) --retireBacklogSize];
    }
}
