#include "engine/Meter.h"
#include "engine/PatternEdit.h"
#include "engine/OutputGuard.h"
#include "engine/PatternPlayer.h"
#include "engine/TabParser.h"
#include "engine/MeterCatalog.h"
#include "engine/Resampler.h"
#include "engine/SamplePlayer.h"
#include "engine/Sequencer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace hz;

static int failures = 0;

#define CHECK(cond) \
    do { if (! (cond)) { std::printf ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

struct AbsEvent { long long sample; ClickEvent e; };

// Läuft den Sequencer in Blöcken und liefert absolute Sampleposition jedes Pulses
static std::vector<AbsEvent> run (Sequencer& seq, long long totalSamples, int blockSize)
{
    std::vector<AbsEvent> result;
    ClickEvent buf[64];

    for (long long done = 0; done < totalSamples; done += blockSize)
    {
        const int len = (int) std::min<long long> (blockSize, totalSamples - done);
        int n = seq.process (len, buf, 64);
        for (int i = 0; i < n; ++i)
            result.push_back ({ done + buf[i].sampleOffset, buf[i] });
    }
    return result;
}

static void testDefaultGroupings()
{
    auto m78 = Meter::make (7, 8);
    CHECK (m78.numGroups == 3 && m78.groups[0] == 2 && m78.groups[1] == 2 && m78.groups[2] == 3);

    auto m68 = Meter::make (6, 8);
    CHECK (m68.numGroups == 2 && m68.groups[0] == 3 && m68.groups[1] == 3);

    auto m54 = Meter::make (5, 4);
    CHECK (m54.numGroups == 5 && m54.isValid());

    auto m1316 = Meter::make (13, 16, { 3, 3, 3, 4 });
    CHECK (m1316.isValid());

    CHECK (! Meter::make (7, 8, { 3, 3 }).isValid());
}

static void testAccents()
{
    auto m = Meter::make (7, 8, { 2, 2, 3 });
    CHECK (m.accentAt (0) == Meter::Accent::Downbeat);
    CHECK (m.accentAt (1) == Meter::Accent::Weak);
    CHECK (m.accentAt (2) == Meter::Accent::Group);
    CHECK (m.accentAt (4) == Meter::Accent::Group);
    CHECK (m.accentAt (5) == Meter::Accent::Weak);
    CHECK (m.accentAt (6) == Meter::Accent::Weak);
}

static void testFourFourTiming()
{
    Sequencer seq;
    seq.prepare (48000.0);
    seq.setTempo (120.0);
    seq.setMeter (Meter::make (4, 4), 0);
    seq.start();

    auto ev = run (seq, 96000, 512);
    CHECK (ev.size() == 4);
    for (size_t i = 0; i < ev.size(); ++i)
        CHECK (ev[i].sample == (long long) i * 24000);

    CHECK (ev[0].e.accent == Meter::Accent::Downbeat);
    CHECK (ev[1].e.accent == Meter::Accent::Group);
}

// 6/8 bei ♩=120: Achtel = 12000 Samples (E2: Vierteltempo bleibt, Achtel konstant)
static void testSixEight()
{
    Sequencer seq;
    seq.prepare (48000.0);
    seq.setTempo (120.0);
    seq.setMeter (Meter::make (6, 8), 0);
    seq.start();

    auto ev = run (seq, 72000, 256);
    CHECK (ev.size() == 6);
    CHECK (ev[1].sample == 12000);
    CHECK (ev[3].e.accent == Meter::Accent::Group);
    CHECK (ev[4].e.accent == Meter::Accent::Weak);
}

// Taktwechsel mitten im Takt greift erst am nächsten Taktanfang
static void testMeterChangeAtBarLine()
{
    Sequencer seq;
    seq.prepare (48000.0);
    seq.setTempo (120.0);
    seq.setMeter (Meter::make (4, 4), 1);
    seq.start();

    ClickEvent buf[64];
    seq.process (30000, buf, 64);               // mitten in Takt 1
    seq.setMeter (Meter::make (7, 8), 2);

    auto ev = run (seq, 66000 + 7 * 12000, 500);
    // restliche Viertel von Takt 1 (Pulse 2,3) bei 48000, 72000 -> relativ 18000, 42000
    CHECK (ev.size() >= 9);
    CHECK (ev[0].e.meterTag == 1 && ev[0].e.pulse == 2);
    CHECK (ev[1].e.meterTag == 1 && ev[1].e.pulse == 3);
    CHECK (ev[2].e.meterTag == 2 && ev[2].e.pulse == 0 && ev[2].e.bar == 1);
    CHECK (ev[2].sample == 96000 - 30000);
    CHECK (ev[3].sample - ev[2].sample == 12000);
}

// Kein Drift über lange Strecken bei krummem Tempo und Blockgröße
static void testNoDrift()
{
    Sequencer seq;
    seq.prepare (44100.0);
    seq.setTempo (133.0);
    seq.setMeter (Meter::make (4, 4), 0);
    seq.start();

    const double spp = 44100.0 * 60.0 / 133.0;
    auto ev = run (seq, (long long) (spp * 4000), 480);
    CHECK (ev.size() >= 3999);

    bool ok = true;
    for (size_t i = 0; i < ev.size(); ++i)
        if (std::abs ((double) ev[i].sample - std::floor ((double) i * spp)) > 1.0)
            ok = false;
    CHECK (ok);
}

// Wechselwunsch während des letzten Pulses (nach dessen Erzeugung) greift trotzdem an der nächsten Eins
static void testMeterChangeDuringLastPulse()
{
    Sequencer seq;
    seq.prepare (48000.0);
    seq.setTempo (120.0);
    seq.setMeter (Meter::make (4, 4), 1);
    seq.start();

    ClickEvent buf[64];
    seq.process (80000, buf, 64);               // Schlag 4 (bei 72000) ist schon erzeugt
    seq.setMeter (Meter::make (6, 8), 2);

    auto ev = run (seq, 40000, 500);
    CHECK (! ev.empty());
    CHECK (ev[0].sample == 96000 - 80000);
    CHECK (ev[0].e.meterTag == 2 && ev[0].e.pulse == 0 && ev[0].e.bar == 1);
}

// Samplerate-Wechsel mitten im Puls: Restzeit wird umgerechnet
static void testSampleRateChange()
{
    Sequencer seq;
    seq.prepare (48000.0);
    seq.setTempo (120.0);
    seq.start();

    ClickEvent buf[64];
    seq.process (12000, buf, 64);               // 12000 Samples bis zum nächsten Viertel übrig
    seq.prepare (96000.0);

    auto ev = run (seq, 30000, 512);
    CHECK (! ev.empty());
    CHECK (ev[0].sample == 24000);
}

// Impuls bleibt beim Umrechnen an derselben Zeitposition (keine Verzögerung)
static void testResamplerNoDelay()
{
    std::vector<float> in (4410, 0.0f);
    in[1000] = 1.0f;
    auto out = resample (in.data(), (int) in.size(), 44100.0, 48000.0);

    int peakAt = 0;
    for (size_t i = 0; i < out.size(); ++i)
        if (std::abs (out[i]) > std::abs (out[(size_t) peakAt]))
            peakAt = (int) i;

    const double expected = 1000.0 * 48000.0 / 44100.0;
    CHECK (std::abs (peakAt - expected) <= 1.0);
    CHECK (out.size() >= (size_t) (4410 * 48000 / 44100));
}

// Sinus behält Frequenz und Amplitude
static void testResamplerSine()
{
    const double f = 1000.0;
    std::vector<float> in (44100);
    for (size_t i = 0; i < in.size(); ++i)
        in[i] = (float) std::sin (2.0 * 3.14159265358979 * f * (double) i / 44100.0);

    auto out = resample (in.data(), (int) in.size(), 44100.0, 48000.0);

    double maxErr = 0.0;
    for (int i = 2000; i < 40000; ++i)
        maxErr = std::max (maxErr, std::abs (out[(size_t) i] - std::sin (2.0 * 3.14159265358979 * f * i / 48000.0)));
    CHECK (maxErr < 0.01);
}

// Downsampling dämpft Frequenzen oberhalb der neuen Nyquist-Grenze (Anti-Aliasing)
static void testResamplerAntiAlias()
{
    const double f = 30000.0;   // über 22050 Hz
    std::vector<float> in (96000);
    for (size_t i = 0; i < in.size(); ++i)
        in[i] = (float) std::sin (2.0 * 3.14159265358979 * f * (double) i / 96000.0);

    auto out = resample (in.data(), (int) in.size(), 96000.0, 44100.0);

    double peak = 0.0;
    for (int i = 2000; i < 40000; ++i)
        peak = std::max (peak, (double) std::abs (out[(size_t) i]));
    CHECK (peak < 0.02);
}

static DrumInstrument makeInstrument (int n)
{
    DrumInstrument inst;
    for (int i = 0; i < n; ++i)
    {
        DrumSample s;
        s.left.assign (10, 0.0f);
        s.right.assign (10, 0.0f);
        s.power = 0.01f * std::pow (1.5f, (float) i);   // Lautheit wächst über die Aufnahmen
        inst.samples.push_back (s);
    }
    DrumKit kit;
    kit.instruments.push_back (inst);
    kit.finalise();
    return kit.instruments[0];
}

// Lautere Velocity wählt nie eine leisere Lautstärkestufe (im Mittel), keine direkte Wiederholung
static void testVelocityPick()
{
    auto inst = makeInstrument (20);

    double prevMean = -1.0;
    for (float v = 0.0f; v <= 1.0f; v += 0.1f)
    {
        double sum = 0.0;
        int last = -1;
        bool repeated = false;
        for (std::uint32_t r = 1; r < 200; ++r)
        {
            auto p = pickForVelocity (inst, v, last, r * 2654435761u);
            repeated = repeated || p.sample == last;
            last = p.sample;
            sum += p.sample;
        }
        const double mean = sum / 199.0;
        CHECK (mean >= prevMean - 0.01);
        CHECK (! repeated);
        prevMean = mean;
    }

    // ganz leise: leiseste Aufnahme, aber zusätzlich abgesenkt (Ghost-Note)
    auto ghost = pickForVelocity (inst, 0.0f, -1, 7);
    CHECK (ghost.sample <= 2);
    CHECK (ghost.gain < 0.6f);

    // ganz laut: lauteste Region, Gain ~1
    auto loud = pickForVelocity (inst, 1.0f, -1, 7);
    CHECK (loud.sample >= 17);
    CHECK (loud.gain > 0.7f && loud.gain < 1.5f);
}

// Mehr Schläge als Stimmen: kein Absturz, Stimmenzahl begrenzt
static void testVoiceStealing()
{
    DrumKit kit;
    DrumInstrument inst;
    DrumSample s;
    s.left.assign (48000, 0.5f);
    s.right.assign (48000, 0.5f);
    s.power = 1.0f;
    inst.samples.push_back (s);
    kit.instruments.push_back (inst);
    kit.finalise();

    SamplePlayer player;
    player.prepare (48000.0);
    player.setKit (&kit);

    std::vector<float> l (256), r (256);
    for (int i = 0; i < SamplePlayer::numVoices + 10; ++i)
        player.trigger (0, 1.0f, i);
    player.render (l.data(), r.data(), 256);
    CHECK (player.getNumActiveVoices() == SamplePlayer::numVoices);
}

static int countMeters (const std::vector<Meter>& cat, int num, int den)
{
    int n = 0;
    for (auto& m : cat)
        if (m.numerator == num && m.denominator == den)
            ++n;
    return n;
}

static void testMeterCatalog()
{
    auto cat = buildMeterCatalog();

    bool allValid = true;
    for (auto& m : cat)
        allValid = allValid && m.isValid();
    CHECK (allValid);

    CHECK (countMeters (cat, 4, 4) == 1);
    CHECK (countMeters (cat, 7, 8) == 3);     // 2+2+3, 2+3+2, 3+2+2
    CHECK (countMeters (cat, 9, 8) == 5);     // 3+3+3 + 4 Anordnungen von 2+2+2+3
    CHECK (countMeters (cat, 13, 16) == 4);   // Anordnungen von 3+3+3+4

    // keine Duplikate
    bool dup = false;
    for (size_t i = 0; i < cat.size(); ++i)
        for (size_t k = i + 1; k < cat.size(); ++k)
            dup = dup || sameMeter (cat[i], cat[k]);
    CHECK (! dup);

    // 9/8: 3+3+3 zuerst
    for (auto& m : cat)
        if (m.numerator == 9 && m.denominator == 8)
        {
            CHECK (meterLabel (m) == "9/8 (3+3+3)");
            break;
        }
}

static void testMeterParse()
{
    Meter m;
    CHECK (parseMeter ("17/16:4+4+4+5", m) && m.numerator == 17 && m.numGroups == 4 && m.groups[3] == 5);
    CHECK (parseMeter ("7/8 (3+2+2)", m) && m.groups[0] == 3);
    CHECK (parseMeter ("5/4", m) && m.numGroups == 5);
    CHECK (! parseMeter ("7/8 (3+3)", m));
    CHECK (! parseMeter ("7/7", m));
    CHECK (! parseMeter ("Quatsch", m));
    CHECK (meterLabel (Meter::make (4, 4)) == "4/4");
}

static const char* const skankTab = R"(
name:  Thrash Skank
tags:  thrash, skank
tempo: 220 (180-250)
meter: 4/4
grid:  1/8

cr | X . . . . . . . |
sn | . x . x . x . x |
bd | x . x . x . x . |
)";

static void testTabParser()
{
    auto r = parseTabs (skankTab);
    CHECK (r.error.empty());
    CHECK (r.patterns.size() == 1);
    if (r.patterns.empty()) return;

    const auto& p = r.patterns[0];
    CHECK (p.name == "Thrash Skank" && p.id == "lib.thrash-skank");
    CHECK (p.tempo == 220.0 && p.tempoMin == 180.0 && p.tempoMax == 250.0);
    CHECK (p.lengthInQuarters() == Rational (4));
    CHECK (p.layers.size() == 2);
    CHECK (p.layers[0].events.size() == 5);   // 1 Crash + 4 Snare
    CHECK (p.layers[1].events.size() == 4);   // 4 Kick
    CHECK (p.layers[1].events[1].pos == Rational (1));
    CHECK (p.layers[0].events[0].velocity == 1.0f);

    // Taktwechsel 5/4 + 6/8, Triolen, Fehlermeldungen
    auto r2 = parseTabs ("name: Wechsel\nmeter: 5/4, 6/8 (3+3)\ngrid: 1/4, 1/8\nsn | x . x . x | x . . x . . |\n");
    CHECK (r2.error.empty());
    if (! r2.patterns.empty())
    {
        CHECK (r2.patterns[0].lengthInQuarters() == Rational (8));
        CHECK (r2.patterns[0].layers[0].events[3].pos == Rational (5));
        CHECK (r2.patterns[0].layers[0].events[4].pos == Rational (13, 2));
    }

    auto r3 = parseTabs ("name: Triole\ngrid: 1/12\nhh | x x x x x x x x x x x x |\n");
    CHECK (r3.error.empty() && r3.patterns[0].layers[0].events[1].pos == Rational (1, 3));

    CHECK (parseTabs ("name: Kaputt\ngrid: 1/8\nsn | x . x |\n").error.find ("erwartet 8") != std::string::npos);
    CHECK (parseTabs ("name: Kaputt\nzz | x . . . . . . . . . . . . . . . |\n").error.find ("unbekannte Spur") != std::string::npos);
    CHECK (parseTabs ("name: Kaputt\ngrid: 1/8\nsn | x . o . . . . . |\n").error.find ("hh-Spur") != std::string::npos);
    CHECK (parseTabs ("name: A\ngrid: 1/4\nsn | x x x x |\n---\nname: B\ngrid: 1/4\nbd | x . x . |\n").patterns.size() == 2);
}

static void testPlanSwingAndFlam()
{
    auto r = parseTabs ("name: Swing\ngrid: 1/8\nswing: 1/8 0.667\nhh | x x x x x x x x |\nsn | . . f . . . . . |\n");
    CHECK (r.error.empty());
    if (r.patterns.empty()) return;
    auto plan = buildPlan (r.patterns[0]);

    // Offbeat-Achtel von 0,5 auf 0,667 Viertel verschoben
    CHECK (std::abs (plan.events[1].pos - 0.667) < 1.0e-3);
    CHECK (plan.events[2].pos == 1.0);

    // Flam: Vorschlag + verzögerter Hauptschlag
    int flamParts = 0;
    for (auto& e : plan.events)
        if (e.voice.instrument == Instrument::Snare)
            ++flamParts;
    CHECK (flamParts == 2);
}

static std::vector<PlayEvent> playAll (PatternPlayer& pp, int total, int block, std::vector<PulseEvent>* pulsesOut = nullptr,
                                       std::vector<long long>* absOut = nullptr)
{
    std::vector<PlayEvent> all;
    PlayEvent ev[256];
    PulseEvent pu[256];
    for (int done = 0; done < total; done += block)
    {
        const int len = std::min (block, total - done);
        int ne = 0, np = 0;
        pp.process (len, ev, 256, ne, pu, 256, np);
        for (int i = 0; i < ne; ++i)
        {
            all.push_back (ev[i]);
            if (absOut) absOut->push_back (done + ev[i].sampleOffset);
        }
        if (pulsesOut)
            for (int i = 0; i < np; ++i)
            {
                pulsesOut->push_back (pu[i]);
                pulsesOut->back().sampleOffset += done;
            }
    }
    return all;
}

static void testPatternPlayerTiming()
{
    auto r = parseTabs (skankTab);
    auto p = r.patterns[0];
    p.humanize = 0.0f;
    auto plan = buildPlan (p);

    PatternPlayer pp;
    pp.prepare (48000.0);
    pp.setTempo (120.0);   // Viertel = 24000 Samples
    pp.setNextPlan (&plan);
    pp.start();

    std::vector<long long> abs;
    std::vector<PulseEvent> pulses;
    auto ev = playAll (pp, 2 * 96000, 500, &pulses, &abs);

    CHECK (ev.size() == 18);                           // 9 Schläge pro Takt, 2 Durchläufe
    CHECK (abs.size() == 18 && abs[0] == 0);
    bool kickOk = true;
    int kicks = 0;
    for (size_t i = 0; i < ev.size(); ++i)
        if (ev[i].voice.instrument == Instrument::Kick)
            kickOk = kickOk && abs[i] == (long long) kicks++ * 24000;
    CHECK (kickOk);

    CHECK (pulses.size() == 8);
    CHECK (pulses[4].loop == 1 && pulses[4].pulse == 0 && pulses[4].sampleOffset == 96000);
}

static void testPatternPlayerSwitchAtEnd()
{
    auto a = parseTabs ("name: A\ngrid: 1/4\nsn | x x x x |\n").patterns[0];
    auto b = parseTabs ("name: B\nmeter: 7/8\ngrid: 1/8\nbd | x . x . x . . |\n").patterns[0];
    a.humanize = b.humanize = 0.0f;
    auto planA = buildPlan (a), planB = buildPlan (b);

    PatternPlayer pp;
    pp.prepare (48000.0);
    pp.setTempo (120.0);
    pp.setNextPlan (&planA);
    pp.start();

    PlayEvent ev[64]; PulseEvent pu[64]; int ne = 0, np = 0;
    pp.process (30000, ev, 64, ne, pu, 64, np);   // mitten im ersten Takt
    pp.setNextPlan (&planB);
    CHECK (pp.getCurrentPlan() == &planA);           // noch nicht gewechselt

    std::vector<long long> abs;
    auto rest = playAll (pp, 96000 - 30000 + 10, 512, nullptr, &abs);
    CHECK (pp.getCurrentPlan() == &planB);
    CHECK (pp.popRetired() == &planA);
    CHECK (pp.popRetired() == nullptr);

    // erster B-Schlag (Kick) genau am Ende von A
    bool found = false;
    for (size_t i = 0; i < rest.size(); ++i)
        if (rest[i].voice.instrument == Instrument::Kick)
        {
            CHECK (abs[i] == 96000 - 30000);
            found = true;
            break;
        }
    CHECK (found);
}

// A pending plan takes over at the next bar end, not only at the plan end (long songs)
static void testPlanSwitchAtBarEnd()
{
    auto a = parseTabs ("name: A\ngrid: 1/4\nsn | x x x x | x x x x | x x x x |\n").patterns[0];
    auto b = parseTabs ("name: B\ngrid: 1/4\nbd | x . . . |\n").patterns[0];
    a.humanize = b.humanize = 0.0f;
    auto planA = buildPlan (a), planB = buildPlan (b);

    PatternPlayer pp;
    pp.prepare (48000.0);
    pp.setTempo (120.0);
    pp.setNextPlan (&planA);
    pp.start();

    PlayEvent ev[64]; PulseEvent pu[64]; int ne = 0, np = 0;
    pp.process (30000, ev, 64, ne, pu, 64, np);   // in bar 1 of 3 (one bar = 96000 samples)
    pp.setNextPlan (&planB);

    std::vector<long long> abs;
    auto rest = playAll (pp, 96000 - 30000 + 10, 512, nullptr, &abs);
    CHECK (pp.getCurrentPlan() == &planB);        // switched at the end of bar 1, not of bar 3

    // the abandoned bars 2 and 3 of A must not be squeezed onto the boundary (that was a loud bang)
    int snaresAfterBoundary = 0;
    for (size_t i = 0; i < rest.size(); ++i)
        if (rest[i].voice.instrument == Instrument::Snare && abs[i] >= 96000 - 30000)
            ++snaresAfterBoundary;
    CHECK (snaresAfterBoundary == 0);
    CHECK (rest.size() < 10);

    bool found = false;
    for (size_t i = 0; i < rest.size(); ++i)
        if (rest[i].voice.instrument == Instrument::Kick)
        {
            CHECK (abs[i] == 96000 - 30000);
            found = true;
            break;
        }
    CHECK (found);
}

// The same pattern again (editor opened/closed while a song plays): no jump, no burst
static void testSamePlanKeepsPosition()
{
    auto a = parseTabs ("name: A\ngrid: 1/4\nsn | x x x x | x x x x | x x x x |\n").patterns[0];
    a.humanize = 0.0f;
    auto plan1 = buildPlan (a), plan2 = buildPlan (a);

    PatternPlayer pp;
    pp.prepare (48000.0);
    pp.setTempo (120.0);
    pp.setNextPlan (&plan1);
    pp.start();

    PlayEvent ev[64]; PulseEvent pu[64]; int ne = 0, np = 0;
    pp.process (120000, ev, 64, ne, pu, 64, np);   // into bar 2 (bar 2 starts at 96000)
    pp.setNextPlan (&plan2);

    std::vector<long long> abs;
    auto rest = playAll (pp, 48000, 512, nullptr, &abs);
    CHECK (pp.getCurrentPlan() == &plan2);         // taken over at once
    // the snares continue on the beat grid of bar 2: 120000 -> next beat at 120000 (beat 2 of bar 2)
    CHECK (! rest.empty());
    for (size_t i = 0; i < abs.size(); ++i)
        CHECK ((abs[i] + 120000) % 24000 == 0);    // every hit still on a quarter of the old timeline
    CHECK (rest.size() == 2);                      // 48000 samples = two quarters, nothing squeezed
}

// Speaker protection: loud bursts are limited, NaN becomes silence, normal levels pass unchanged
static void testOutputGuard()
{
    OutputGuard g;
    g.prepare (48000.0);

    float l[512], r[512];
    for (int i = 0; i < 512; ++i) { l[i] = 0.3f; r[i] = -0.3f; }
    g.process (l, r, 512);
    CHECK (std::abs (l[100] - 0.3f) < 1.0e-6f && std::abs (r[100] + 0.3f) < 1.0e-6f);

    for (int i = 0; i < 512; ++i) { l[i] = (i % 2 ? 4.0f : -4.0f); r[i] = 2.0f; }
    l[7] = std::nanf (""); r[9] = INFINITY;
    g.process (l, r, 512);
    bool ok = true;
    for (int i = 0; i < 512; ++i)
        if (! std::isfinite (l[i]) || ! std::isfinite (r[i]) || std::abs (l[i]) > OutputGuard::ceiling + 1.0e-6f
            || std::abs (r[i]) > OutputGuard::ceiling + 1.0e-6f)
            ok = false;
    CHECK (ok);
    CHECK (l[7] == 0.0f && r[9] == 0.0f);

    // after the burst the gain recovers within a few hundred milliseconds
    float quietL[4800], quietR[4800];
    for (int k = 0; k < 10; ++k)
    {
        for (int i = 0; i < 4800; ++i) { quietL[i] = 0.1f; quietR[i] = 0.1f; }
        g.process (quietL, quietR, 4800);
    }
    CHECK (g.currentGain() > 0.99f);
}

// A seek while a plan waits lands in that plan ("play the arrangement from part 3")
static void testSeekIntoPendingPlan()
{
    auto a = parseTabs ("name: A\ngrid: 1/4\nsn | x x x x | x x x x |\n").patterns[0];
    auto b = parseTabs ("name: B\ngrid: 1/4\nbd | x . . . | x . . . | x . . . | x . . . |\n").patterns[0];
    a.humanize = b.humanize = 0.0f;
    auto planA = buildPlan (a), planB = buildPlan (b);

    PatternPlayer pp;
    pp.prepare (48000.0);
    pp.setTempo (120.0);
    pp.setNextPlan (&planA);
    pp.start();
    PlayEvent ev[64]; PulseEvent pu[64]; int ne = 0, np = 0;
    pp.process (30000, ev, 64, ne, pu, 64, np);
    pp.setNextPlan (&planB);
    pp.seek (8.0);   // bar 3 of B (quarters)

    std::vector<PulseEvent> pulses;
    playAll (pp, 96000, 512, &pulses);
    CHECK (pp.getCurrentPlan() == &planB);

    // the first pulse of B (4 bars) is bar 2, never bar 0
    int firstBarOfB = -1;
    for (auto& e : pulses)
        if (e.numBars == 4) { firstBarOfB = e.bar; break; }
    CHECK (firstBarOfB == 2);
}

static void testHumanizeDeterministicAndBounded()
{
    auto p = parseTabs (skankTab).patterns[0];
    p.humanize = 1.0f;
    auto plan = buildPlan (p);

    auto run = [&]
    {
        PatternPlayer pp;
        pp.prepare (48000.0);
        pp.setTempo (240.0);
        pp.setNextPlan (&plan);
        pp.start();
        std::vector<long long> abs;
        playAll (pp, 48000 * 4, 480, nullptr, &abs);
        return abs;
    };

    auto first = run(), second = run();
    CHECK (first == second);                          // reproduzierbar

    // Durchlauf 1 und 2 unterscheiden sich (anderer Zufall je Durchlauf)
    const long long loopLen = 48000;                 // 4 Viertel bei 240 BPM
    bool differs = false;
    for (size_t i = 0; i + 9 < first.size(); ++i)
        differs = differs || (first[i + 9] - loopLen) != first[i];
    CHECK (differs);

    // nichts vor dem Pattern-Anfang, Abweichung <= 12 ms
    CHECK (! first.empty() && first[0] >= 0);
    CHECK (std::abs (first[2] - 6000) <= 12 * 48 + 1);   // dritter Schlag (Snare auf der Achtel) = 6000 Samples
}

// 3 gegen 2: eigene Teilung pro Spur
static void testPolyrhythmLane()
{
    auto r = parseTabs ("name: Drei gegen Zwei\ngrid: 1/8\ncr(3) | X x x |\nsn | x . . . x . . . |\nbd(5) | x x x x x |\n");
    CHECK (r.error.empty());
    if (r.patterns.empty()) return;
    const auto& hands = r.patterns[0].layers[0].events;
    CHECK (hands.size() == 5);
    CHECK (hands[1].pos == Rational (4, 3));
    CHECK (hands[2].pos == Rational (8, 3));
    CHECK (r.patterns[0].layers[1].events[1].pos == Rational (4, 5));
    CHECK (parseTabs ("name: X\ncr(3) | x x |\n").error.find ("erwartet 3") != std::string::npos);
}

// Tempowechsel mitten im Pattern bei starker Humanisierung: jeder Schlag genau einmal
static void testTempoChangeExactlyOnce()
{
    auto p = parseTabs ("name: Dicht\ngrid: 1/16\nhh | x x x x x x x x x x x x x x x x |\n").patterns[0];
    p.humanize = 1.0f;
    auto plan = buildPlan (p);

    PatternPlayer pp;
    pp.prepare (48000.0);
    pp.setTempo (200.0);
    pp.setNextPlan (&plan);
    pp.start();

    PlayEvent ev[256]; PulseEvent pu[256];
    int total = 0;
    double tempo = 200.0;
    bool reachedLoop3 = false;
    // Tempo springt bei jedem Block zwischen 200 und 90; zählen bis zum Beginn des 4. Durchlaufs
    for (int block = 0; block < 100000 && ! reachedLoop3; ++block)
    {
        int ne = 0, np = 0;
        pp.setTempo (tempo);
        pp.process (64, ev, 256, ne, pu, 256, np);

        int limit = 64;
        for (int i = 0; i < np; ++i)
            if (pu[i].loop == 3) { limit = pu[i].sampleOffset; reachedLoop3 = true; break; }
        for (int i = 0; i < ne; ++i)
            if (ev[i].sampleOffset < limit)
                ++total;

        tempo = tempo > 150.0 ? 90.0 : 200.0;
    }
    CHECK (reachedLoop3);
    CHECK (total == 48);
}

// Positiv humanisierte letzte Note geht am Pattern-Ende nicht verloren
static void testLastNoteKept()
{
    auto p = parseTabs ("name: Ende\ngrid: 1/16\nsn | . . . . . . . . . . . . . . . x |\n").patterns[0];
    p.humanize = 1.0f;
    auto plan = buildPlan (p);

    int count = 0;
    for (std::uint32_t seed = 1; seed < 200; ++seed)
    {
        plan.seed = seed;
        PatternPlayer pp;
        pp.prepare (48000.0);
        pp.setTempo (300.0);
        pp.setNextPlan (&plan);
        pp.start();
        PlayEvent ev[16]; PulseEvent pu[64];
        int ne = 0, np = 0;
        pp.process (38400, ev, 16, ne, pu, 64, np);  // genau ein Durchlauf (4 Viertel à 9600 Samples bei 300 BPM)
        count += ne;
    }
    CHECK (count == 199);
}

static size_t countEvents (const Pattern& p)
{
    size_t n = 0;
    for (auto& l : p.layers) n += l.events.size();
    return n;
}

static void testPatternEdit()
{
    auto p = parseTabs ("name: E\ngrid: 1/8\nsn | . x . x . x . x |\nbd | x . x . x . x . |\n").patterns[0];
    const Voice snare { Instrument::Snare, Articulation::Normal };
    const Voice kick { Instrument::Kick, Articulation::Normal };
    const Voice ride { Instrument::Ride, Articulation::Normal };

    // setzen / entfernen
    CHECK (toggleEvent (p, ride, Rational (0), 0.75f));
    CHECK (findEvent (p, ride, Rational (0)) != nullptr);
    CHECK (! toggleEvent (p, ride, Rational (0), 0.75f));
    CHECK (findEvent (p, ride, Rational (0)) == nullptr);
    CHECK (! toggleEvent (p, ride, Rational (4), 0.75f));          // außerhalb
    CHECK (p.validate().empty());

    // Füße landen im Füße-Layer, IDs eindeutig
    toggleEvent (p, kick, Rational (1, 2), 0.5f);
    bool inFeet = false;
    for (auto& l : p.layers)
        if (l.role == LayerRole::Feet)
            inFeet = std::any_of (l.events.begin(), l.events.end(), [] (auto& e) { return e.pos == Rational (1, 2); });
    CHECK (inFeet);
    CHECK (p.validate().empty());

    CHECK (setEventVelocity (p, snare, Rational (1, 2), 0.2f));
    CHECK (findEvent (p, snare, Rational (1, 2))->velocity == 0.2f);

    // Takt mit Inhalt kopieren, dann wieder entfernen
    const auto before = countEvents (p);
    insertBar (p, 0, true);
    CHECK (p.bars.size() == 2 && p.lengthInQuarters() == Rational (8));
    CHECK (countEvents (p) == before * 2);
    CHECK (findEvent (p, snare, Rational (9, 2)) != nullptr);
    CHECK (p.validate().empty());
    CHECK (removeBar (p, 1));
    CHECK (countEvents (p) == before);
    CHECK (! removeBar (p, 0));                                       // letzter Takt bleibt

    // Taktart verkleinern: Schläge hinten fallen weg, Folgetakt rückt nach
    insertBar (p, 0, true);
    setBarMeter (p, 0, Meter::make (3, 4));
    CHECK (p.lengthInQuarters() == Rational (7));
    int snaresBar1 = 0;                                               // Takt 1 (jetzt 3/4) behält 0,5 1,5 2,5
    for (auto& l : p.layers)
        for (auto& e : l.events)
            if (e.voice == snare && e.pos < Rational (3))
                ++snaresBar1;
    CHECK (snaresBar1 == 3);
    CHECK (findEvent (p, snare, Rational (7, 2)) != nullptr);        // Takt-2-Snare (4,5) rückt auf 3,5
    CHECK (findEvent (p, kick, Rational (3)) != nullptr);            // Takt 2 beginnt jetzt bei 3
    CHECK (p.validate().empty());

    // Raster muss aufgehen
    setBarMeter (p, 1, Meter::make (7, 8));
    setBarGrid (p, 1, Rational (1, 4));                               // 7/8 in Vierteln geht nicht
    CHECK (p.bars[1].grid != Rational (1, 4));
    CHECK (p.validate().empty());
}

// Seek: playback continues from the new position (here: bar 2 of a 2-bar pattern)
static void testSeek()
{
    auto p = parseTabs ("name: S\ngrid: 1/4\nsn | x . . . | . . x . |\n").patterns[0];
    p.humanize = 0.0f;
    auto plan = buildPlan (p);

    PatternPlayer pp;
    pp.prepare (48000.0);
    pp.setTempo (120.0);
    pp.setNextPlan (&plan);
    pp.seek (4.0);          // while stopped: start in bar 2
    pp.start();

    PlayEvent ev[16]; PulseEvent pu[16]; int ne = 0, np = 0;
    pp.process (60000, ev, 16, ne, pu, 16, np);
    CHECK (ne == 1 && ev[0].sampleOffset == 48000);   // snare on beat 3 of bar 2 = 2 quarters later
    CHECK (np >= 1 && pu[0].bar == 1 && pu[0].pulse == 0);

    pp.seek (0.0);          // while playing: back to the start
    pp.process (100, ev, 16, ne, pu, 16, np);
    CHECK (ne == 1 && ev[0].sampleOffset == 0);
}

// Stop pauses: start continues at the same position; seek (0) rewinds
static void testPauseResume()
{
    auto p = parseTabs ("name: P\ngrid: 1/4\nsn | x x x x |\n").patterns[0];
    p.humanize = 0.0f;
    auto plan = buildPlan (p);

    PatternPlayer pp;
    pp.prepare (48000.0);
    pp.setTempo (120.0);
    pp.setNextPlan (&plan);
    pp.start();

    PlayEvent ev[16]; PulseEvent pu[16]; int ne = 0, np = 0;
    pp.process (30000, ev, 16, ne, pu, 16, np);   // beats 1 and 2 played; now 1.25 quarters in
    pp.stop();
    pp.start();
    pp.process (30000, ev, 16, ne, pu, 16, np);
    CHECK (ne >= 1 && ev[0].sampleOffset == 18000);   // beat 3 (at 2.0) is 0.75 quarters away

    pp.stop();
    pp.seek (0.0);
    pp.start();
    pp.process (100, ev, 16, ne, pu, 16, np);
    CHECK (ne == 1 && ev[0].sampleOffset == 0);
}

int main()
{
    testDefaultGroupings();
    testAccents();
    testFourFourTiming();
    testSixEight();
    testMeterChangeAtBarLine();
    testNoDrift();
    testMeterChangeDuringLastPulse();
    testSampleRateChange();
    testResamplerNoDelay();
    testResamplerSine();
    testResamplerAntiAlias();
    testVelocityPick();
    testVoiceStealing();
    testMeterCatalog();
    testMeterParse();
    testTabParser();
    testPlanSwingAndFlam();
    testPatternPlayerTiming();
    testPatternPlayerSwitchAtEnd();
    testPlanSwitchAtBarEnd();
    testSamePlanKeepsPosition();
    testOutputGuard();
    testSeekIntoPendingPlan();
    testHumanizeDeterministicAndBounded();
    testPolyrhythmLane();
    testTempoChangeExactlyOnce();
    testLastNoteKept();
    testPatternEdit();
    testSeek();
    testPauseResume();

    if (failures == 0)
        std::printf ("Alle Engine-Tests bestanden.\n");

    return failures == 0 ? 0 : 1;
}
