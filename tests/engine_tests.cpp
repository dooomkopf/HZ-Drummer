#include "engine_test_util.h"


int failures = 0;


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

// Pegelanzeige: der Sampler merkt sich den Spitzenpegel je Mixer-Kanal bis zum Zurücksetzen
static void testChannelPeaks()
{
    DrumKit kit;
    DrumInstrument inst;
    DrumSample s;
    s.left.assign (1000, 0.5f);
    s.right.assign (1000, 0.5f);
    s.power = 1.0f;
    inst.samples.push_back (s);
    kit.instruments.push_back (inst);
    kit.finalise();

    SamplePlayer player;
    player.prepare (48000.0);
    player.setKit (&kit);

    std::vector<float> l (256), r (256);
    player.trigger (0, 1.0f, 10, 3);
    player.render (l.data(), r.data(), 256);
    CHECK (player.peak (3) > 0.1f);
    CHECK (player.peak (0) <= 0.0f);
    player.resetPeaks();
    CHECK (player.peak (3) <= 0.0f);
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
    testChannelPeaks();
    testMeterCatalog();
    testMeterParse();
    testTabParser();
    runPlayerTests();   // engine_tests_player.cpp

    if (failures == 0)
        std::printf ("Alle Engine-Tests bestanden.\n");

    return failures == 0 ? 0 : 1;
}
