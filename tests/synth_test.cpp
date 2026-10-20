// Checks on the MS-20 style synth engine. Runs automatically on every build.
#include <cmath>
#include <cstdio>
#include "synth/Ms20.h"
#include "dsp/Preamp.h"
#include "dsp/ToneStack.h"
#include "dsp/PowerAmp.h"

using namespace ms20;

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

static const double fs = 384000.0; // 8x oversampled rate at 48 kHz

static Params cleanPatch()
{
    Params p;
    p.vco1Wave = 1; p.vco1Oct = 0; p.level1 = 1.0; p.level2 = 0.0;
    p.hpCutoff = 20.0; p.hpRes = 0.0;
    p.lpCutoff = 20000.0; p.lpRes = 0.0;
    p.eg2Attack = 0.001; p.eg2Sustain = 1.0; p.eg2Release = 0.05;
    p.outLevel = 1.0;
    return p;
}

// Frequency of the voice's output, by counting upward zero crossings.
static double measureHz (Voice& v, double seconds)
{
    int crossings = 0; double prev = 0.0;
    const int skip = (int) (fs * 0.05), n = (int) (fs * seconds);
    for (int i = 0; i < skip; ++i) prev = v.render (0.0);
    for (int i = 0; i < n; ++i)
    {
        const double y = v.render (0.0);
        if (prev < 0.0 && y >= 0.0) ++crossings;
        prev = y;
    }
    return crossings / seconds;
}

int main()
{
    // 1. Pitch: footage switches and VCO2 land on the right notes.
    {
        Voice v; v.prepare (fs); Params p = cleanPatch(); v.setParams (p);
        v.noteOn (69);
        check (std::fabs (measureHz (v, 1.0) - 440.0) < 1.5, "VCO1 8' plays A4 at 440 Hz");

        p.vco1Oct = -1; v.setParams (p);
        check (std::fabs (measureHz (v, 1.0) - 220.0) < 1.5, "VCO1 16' drops an octave (220 Hz)");

        p.level1 = 0.0; p.level2 = 1.0; p.vco2Wave = 1; p.vco2Oct = 1; v.reset(); v.setParams (p); v.noteOn (69);
        check (std::fabs (measureHz (v, 1.0) - 880.0) < 2.0, "VCO2 4' square plays 880 Hz");
    }

    // 2. Keyboard: lowest held key wins, like the MS-20.
    {
        Voice v; v.prepare (fs); v.setParams (cleanPatch());
        v.noteOn (60); v.noteOn (64);
        const bool a = v.targetSemis == 60.0;
        v.noteOff (60);
        const bool b = v.targetSemis == 64.0;
        v.noteOn (55);
        const bool c = v.targetSemis == 55.0;
        check (a && b && c, "low-note priority");
    }

    // 3. Envelope: release fades to silence.
    {
        Voice v; v.prepare (fs); Params p = cleanPatch(); p.eg2Release = 0.05; v.setParams (p);
        v.noteOn (57);
        double loud = 0.0;
        for (int i = 0; i < (int) (fs * 0.2); ++i) loud = std::fmax (loud, std::fabs (v.render (0.0)));
        v.noteOff (57);
        for (int i = 0; i < (int) (fs * 0.3); ++i) v.render (0.0);
        double tail = 0.0;
        for (int i = 0; i < (int) (fs * 0.05); ++i) tail = std::fmax (tail, std::fabs (v.render (0.0)));
        check (loud > 0.3 && tail < 1.0e-3, "EG2 release fades to silence");
    }

    // 4. The filter alone can be played as an oscillator (resonance at max, VCOs off).
    for (int rev = 0; rev < 2; ++rev)
    {
        Voice v; v.prepare (fs);
        Params p = cleanPatch(); p.level1 = 0.0; p.lpCutoff = 1000.0; p.lpRes = 1.0; p.rev2 = rev == 1;
        v.setParams (p);
        v.noteOn (60);
        double peak = 0.0;
        for (int i = 0; i < (int) (fs * 0.5); ++i) peak = std::fmax (peak, std::fabs (v.render (0.0)));
        check (peak > 0.02 && peak < 2.0, rev == 0 ? "Rev1 filter self-oscillates, stays bounded"
                                                   : "Rev2 filter self-oscillates, stays bounded");
    }

    // 5. Worst case: everything maxed, feedback patched through a cranked amp.
    for (int rev = 0; rev < 2; ++rev)
    {
        Voice v; v.prepare (fs);
        Params p;
        p.vco1Wave = 3; p.vco2Wave = 3; p.level1 = p.level2 = 1.0;
        p.hpRes = p.lpRes = 1.0; p.hpMgOct = p.lpMgOct = 4.0; p.hpEgOct = p.lpEgOct = 7.0;
        p.fmMgSemis = 24.0; p.fmEg1Semis = 48.0; p.mgFreq = 20.0; p.sampleHold = true;
        p.feedback = 1.5; p.outLevel = 0.6; p.rev2 = rev == 1; p.pwm = 0.45;
        v.setParams (p);

        t150::Preamp pre; t150::ToneStack ts; t150::PowerAmp pa;
        pre.prepare (fs); ts.prepare (fs); pa.prepare (fs); ts.setControls (0.5f, 0.5f, 0.5f); pa.setSag (1.0f);

        bool finite = true; double peak = 0.0, last = 0.0;
        for (int i = 0; i < (int) (fs * 2.0); ++i)
        {
            if (i % 20000 == 0) v.noteOn (24 + (i / 20000) % 70);
            if (i % 20000 == 10000) v.noteOff (24 + (i / 20000) % 70);
            const double s = v.render (last);
            last = pa.process (ts.process (pre.process ((float) s, 1.0f)));
            if (! std::isfinite (last)) finite = false;
            peak = std::fmax (peak, std::fabs (last));
        }
        check (finite && peak < 4.0, rev == 0 ? "Rev1: maxed-out patch with amp feedback stays stable"
                                              : "Rev2: maxed-out patch with amp feedback stays stable");
    }

    std::printf ("\n%s\n", failures == 0 ? "All tests passed." : "Some tests FAILED.");
    return failures == 0 ? 0 : 1;
}
