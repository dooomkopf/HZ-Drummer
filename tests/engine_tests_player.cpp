// Engine tests, part 2: plans, the pattern player, pattern edits, delayed hits, seeking.

#include "engine_test_util.h"

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
// Strg+Klick im Raster: ein Schlag klingt etwas später als sein Platz ("laid back")
static void testLaggedHit()
{
    auto p = parseTabs (skankTab).patterns[0];
    p.humanize = 0.0f;
    Voice kick;
    for (auto& l : p.layers)
        for (auto& e : l.events)
            if (e.voice.instrument == Instrument::Kick && e.pos == Rational (0))
                kick = e.voice;
    CHECK (setEventLag (p, kick, Rational (0), 20.0f));
    CHECK (p.validate().empty());
    auto plan = buildPlan (p);

    PatternPlayer pp;
    pp.prepare (48000.0);
    pp.setTempo (120.0);   // Viertel = 24000 Samples
    pp.setNextPlan (&plan);
    pp.start();
    std::vector<long long> abs;
    auto ev = playAll (pp, 96000, 500, nullptr, &abs);
    std::vector<long long> kicks;
    for (size_t i = 0; i < ev.size(); ++i)
        if (ev[i].voice.instrument == Instrument::Kick)
            kicks.push_back (abs[i]);
    CHECK (kicks.size() >= 2 && kicks[0] == 960 && kicks[1] == 24000);   // 20 ms = 960 Samples, nur der eine Schlag

    // früher (negativ): der Kick auf Zählzeit 2 kommt 40 ms = 1920 Samples vor seinem Platz
    CHECK (setEventLag (p, kick, Rational (1), -40.0f));
    plan = buildPlan (p);
    pp.setNextPlan (&plan);
    pp.start();
    abs.clear();
    ev = playAll (pp, 96000, 500, nullptr, &abs);
    kicks.clear();
    for (size_t i = 0; i < ev.size(); ++i)
        if (ev[i].voice.instrument == Instrument::Kick)
            kicks.push_back (abs[i]);
    CHECK (kicks.size() >= 2 && kicks[1] == 24000 - 1920);

    auto bad = p;
    bad.layers[0].events[0].lagMs = 150.0f;
    CHECK (! bad.validate().empty());
    bad.layers[0].events[0].lagMs = -150.0f;
    CHECK (! bad.validate().empty());

    // Strg+Rechtsklick: Flam an und wieder aus; der Plan bekommt dafür den Vorschlag dazu
    const auto hitsBefore = buildPlan (p).events.size();
    CHECK (setEventFlam (p, kick, Rational (0), true));
    CHECK (buildPlan (p).events.size() == hitsBefore + 1);
    CHECK (setEventFlam (p, kick, Rational (0), false));
    CHECK (buildPlan (p).events.size() == hitsBefore);
    CHECK (! setEventFlam (p, kick, Rational (1, 3), true));   // dort liegt kein Schlag
}

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

void runPlayerTests()
{
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
    testLaggedHit();
    testSeek();
    testPauseResume();
}
