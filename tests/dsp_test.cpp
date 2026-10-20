// Quick checks on the amp's DSP. Runs automatically on every GitHub build.
// Returns 0 if everything passes, 1 if anything fails.
#include <cmath>
#include <cstdio>
#include "dsp/ToneStack.h"
#include "dsp/Preamp.h"
#include "dsp/PowerAmp.h"
#include "dsp/CabSim.h"

using namespace t150;

static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

// Level (dB) of a steady sine through the tone stack.
static double toneStackDb (float bass, float mid, float treble, double freq)
{
    const double fs = 192000.0;
    ToneStack ts;
    ts.prepare (fs);
    ts.setControls (bass, mid, treble);
    double peak = 0.0;
    const int n = (int) (fs * 0.5);
    for (int i = 0; i < n; ++i)
    {
        const float y = ts.process ((float) std::sin (2.0 * kPi * freq * i / fs));
        if (i > n / 2) peak = std::fmax (peak, std::fabs (y));
    }
    return 20.0 * std::log10 (peak);
}

int main()
{
    // 1. Tone stack matches the real circuit.
    //    Reference values come from solving the Bassman tone stack circuit
    //    directly (all knobs at noon, after the pot taper).
    {
        const double freqs[] = { 50, 200, 500, 1000, 3000, 8000 };
        const double ref[]   = { -4.2, -5.9, -10.5, -12.5, -10.8, -9.5 };
        bool ok = true;
        for (int i = 0; i < 6; ++i)
            ok &= std::fabs (toneStackDb (0.5f, 0.5f, 0.5f, freqs[i]) - ref[i]) < 0.3;
        check (ok, "tone stack matches circuit (all knobs at noon)");
    }

    // 2. Knobs do what their names say.
    check (toneStackDb (1.0f, 0.5f, 0.5f, 80)   > toneStackDb (0.0f, 0.5f, 0.5f, 80)   + 6.0, "bass knob raises lows");
    check (toneStackDb (0.5f, 0.5f, 1.0f, 5000) > toneStackDb (0.5f, 0.5f, 0.0f, 5000) + 6.0, "treble knob raises highs");
    check (toneStackDb (0.5f, 1.0f, 0.5f, 600)  > toneStackDb (0.5f, 0.0f, 0.5f, 600)  + 3.0, "mid knob raises mids");

    // 3. Whole amp chain, fully cranked, stays stable: no blow-ups, no clicks
    //    of DC offset, output stays in a sane range.
    {
        const double fs = 384000.0; // 8x oversampled rate at 48 kHz
        Preamp pre; ToneStack ts; PowerAmp pa;
        pre.prepare (fs); ts.prepare (fs); pa.prepare (fs);
        ts.setControls (0.5f, 0.5f, 0.5f);
        pa.setSag (1.0f);

        bool finite = true;
        double peak = 0.0, sum = 0.0;
        const int n = (int) fs;
        for (int i = 0; i < n; ++i)
        {
            const float x = 0.8f * (float) std::sin (2.0 * kPi * 82.41 * i / fs);
            const float y = pa.process (ts.process (pre.process (x, 1.0f)));
            if (! std::isfinite (y)) finite = false;
            if (i > n / 2) { peak = std::fmax (peak, std::fabs (y)); sum += y; }
        }
        check (finite, "cranked amp never produces NaN/inf");
        check (peak > 0.1 && peak < 2.0, "cranked amp output level is sane");
        check (std::fabs (sum / (n / 2)) < 0.01, "no DC offset at the output");
    }

    // 4. Silence in, silence out (no self-noise or offset).
    {
        Preamp pre; ToneStack ts; PowerAmp pa; CabSim cab;
        pre.prepare (192000.0); ts.prepare (192000.0); pa.prepare (192000.0); cab.prepare (48000.0);
        ts.setControls (0.5f, 0.5f, 0.5f);
        double peak = 0.0;
        for (int i = 0; i < 48000; ++i)
            peak = std::fmax (peak, std::fabs (cab.process (pa.process (ts.process (pre.process (0.0f, 1.0f))))));
        check (peak < 1.0e-6, "silence in gives silence out");
    }

    std::printf ("\n%s\n", failures == 0 ? "All tests passed." : "Some tests FAILED.");
    return failures == 0 ? 0 : 1;
}
