#pragma once
// Power amp: four 6550s in push-pull, ultralinear, into a big output
// transformer.
//
// What matters for the sound:
//  - This is where most of the Model T's thick distortion lives. Clipping
//    AFTER the tone stack, with the full low end intact, is what gives that
//    wall-of-sound weight.
//  - Push-pull clipping is symmetric (odd harmonics), and it's a rounded,
//    gradual clip rather than a hard wall.
//  - "Sag": when you hit it hard, the power supply voltage dips, the amp's
//    ceiling drops for a moment, and you get that squashy, blooming feel.
//  - The output transformer rounds off the top end, so the fizz from
//    clipping never reaches the speaker at full strength.
//
// v2: rounded clip curve with anti-aliasing (ADAA), filtering before and
// after the clip, more drive range, gentler sag.
//
// Runs at the oversampled rate.
#include <cmath>
#include "Filters.h"

namespace t150
{
class PowerAmp
{
public:
    float drive = 16.0f;   // how hard the master volume can push the tubes

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        using T = Biquad::Type;
        // Phase inverter / driver stage: tames highs before the power tubes.
        driverLp.set (T::lowPass, sampleRate, 6500.0, 0.6);
        // Output transformer: gentle low cut and a rounded-off top end.
        otHp.set (T::highPass, sampleRate, 30.0, 0.6);
        otLp.set (T::lowPass,  sampleRate, 8000.0, 0.6);
        // Tube amps let the speaker's low resonance come through more than
        // solid-state ones do; this bump puts that weight back in.
        lowBump.set (T::peak, sampleRate, 95.0, 1.0, 2.5);
        setSagTimes (8.0, 180.0);
        reset();
    }
    void reset()
    {
        env = 0.0f; uPrev = 0.0; adPrev = 0.0;
        driverLp.reset(); otHp.reset(); otLp.reset(); lowBump.reset();
    }

    // sagAmount 0..1
    void setSag (float amount) { sag = amount < 0.f ? 0.f : (amount > 1.f ? 1.f : amount); }

    float process (float x)
    {
        x = driverLp.process (x) * drive;

        // Follow how hard the amp is working (fast-ish attack, slow recovery).
        const float level = std::fabs (x);
        const float coeff = level > env ? attack : release;
        env += coeff * (level - env);

        // Ceiling drops as the supply sags (gentler than v1 so it doesn't thin out).
        const double ceiling = 1.0 / (1.0 + 0.15 * sag * env);

        double y = ceiling * clipADAA ((double) x / ceiling);
        y = otLp.process ((float) y);
        y = otHp.process ((float) y);
        return lowBump.process ((float) y);
    }

private:
    // Rounded symmetric clip: f(u) = u / sqrt(1 + u^2).
    // Stays clean for small signals, then bends over smoothly.
    static double clip   (double u) { return u / std::sqrt (1.0 + u * u); }
    static double clipAD (double u) { return std::sqrt (1.0 + u * u) - 1.0; } // its antiderivative

    double clipADAA (double u)
    {
        const double ad = clipAD (u);
        const double du = u - uPrev;
        const double y = std::fabs (du) < 1.0e-6 ? clip (0.5 * (u + uPrev)) : (ad - adPrev) / du;
        uPrev = u;
        adPrev = ad;
        return y;
    }

    void setSagTimes (double attackMs, double releaseMs)
    {
        attack  = (float) (1.0 - std::exp (-1.0 / (0.001 * attackMs  * fs)));
        release = (float) (1.0 - std::exp (-1.0 / (0.001 * releaseMs * fs)));
    }

    double fs = 48000.0;
    float sag = 0.5f, env = 0.0f, attack = 0.0f, release = 0.0f;
    double uPrev = 0.0, adPrev = 0.0;
    Biquad driverLp, otHp, otLp, lowBump;
};
} // namespace t150
