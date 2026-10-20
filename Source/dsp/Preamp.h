#pragma once
// Preamp: two 12AX7 triode gain stages, like the Bassman-style front end.
//
//   input -> [V1] -> Volume knob -> [V2] -> (to tone stack)
//
// Each triode stage:  input filter -> gain -> tube curve -> output filter
//
// v2 changes (fixing "harsh, digital" fizz when cranked):
//  - The tube curve uses "antiderivative anti-aliasing" (ADAA). Plain
//    clipping in software creates tones above what the sample rate can hold,
//    and they fold back down as harsh, out-of-tune fizz. ADAA smooths the
//    clipper so far less of that is created in the first place.
//  - A low-pass BEFORE the curve, like a real tube's input capacitance.
//    Real tubes never see the extreme highs at full strength, so they
//    distort a rounder signal.
//  - Lower preamp gain. On a Model T most of the big, thick distortion comes
//    from the power tubes, not the preamp. Squaring things off in the preamp
//    was a big reason it went thin when cranked.
//
// Runs at the oversampled rate (the processor handles that).
#include <cmath>
#include "Filters.h"

namespace t150
{
// One triode's transfer curve. Real triodes clip unevenly: the positive
// side hits a firmer wall (grid starts conducting) while the negative side
// fades out softly (tube approaching cutoff). That unevenness creates
// even harmonics, a big part of the "warm" tube sound.
constexpr double kTriodeSoft = 0.6;

inline double triodeCurve (double x)
{
    if (x >= 0.0)
        return std::tanh (x);
    return x / (1.0 - kTriodeSoft * x);  // x < 0 here, so this is x / (1 + a|x|)
}

// Antiderivative of triodeCurve (needed for ADAA). Continuous at 0.
inline double triodeCurveAD (double x)
{
    if (x >= 0.0)
    {
        // log(cosh(x)), written so it never overflows for big x
        return x + std::log1p (std::exp (-2.0 * x)) - 0.69314718055994531;
    }
    const double a = kTriodeSoft;
    return -x / a - std::log (1.0 - a * x) / (a * a);
}

// First-order ADAA wrapper: instead of f(x), output the average of f over
// the step from the previous sample to this one.
struct AdaaTriode
{
    double xPrev = 0.0, adPrev = 0.0;
    void reset() { xPrev = 0.0; adPrev = 0.0; }
    double process (double x)
    {
        const double ad = triodeCurveAD (x);
        const double dx = x - xPrev;
        double y;
        if (std::fabs (dx) < 1.0e-6)
            y = triodeCurve (0.5 * (x + xPrev));
        else
            y = (ad - adPrev) / dx;
        xPrev = x;
        adPrev = ad;
        return y;
    }
};

class TriodeStage
{
public:
    float gain = 4.0f;    // how hard the signal hits the tube
    float bias = 0.15f;   // shifts the operating point -> more asymmetry

    void prepare (double sampleRate, double inputLpHz, double couplingHz, double rolloffHz)
    {
        inLp1.prepare (sampleRate, inputLpHz);
        inLp2.prepare (sampleRate, inputLpHz * 1.5);
        hp.prepare (sampleRate, couplingHz);
        lp.prepare (sampleRate, rolloffHz);
        restOffset = triodeCurve (bias);
        reset();
    }
    void reset()
    {
        inLp1.reset(); inLp2.reset(); hp.reset(); lp.reset();
        // Start the tube at its resting point, so there's no click on startup.
        adaa.xPrev  = bias;
        adaa.adPrev = triodeCurveAD (bias);
    }

    float process (float x)
    {
        x = inLp2.process (inLp1.process (x));
        double y = adaa.process ((double) x * gain + bias) - restOffset;
        float out = lp.process ((float) y);
        return hp.process (out);  // coupling cap also strips DC created by the clipping
    }

private:
    OnePoleLP inLp1, inLp2;
    OnePoleHP hp;
    OnePoleLP lp;
    AdaaTriode adaa;
    double restOffset = 0.0;
};

class Preamp
{
public:
    void prepare (double sampleRate)
    {
        v1.gain = 4.0f;
        v2.gain = 6.0f;
        //            input LP   coupling   output roll-off
        v1.prepare (sampleRate, 9000.0,   20.0,   10000.0);
        v2.prepare (sampleRate, 7000.0,   25.0,    8000.0);
    }
    void reset() { v1.reset(); v2.reset(); }

    // volume: 0..1 gain fraction (already tapered by the caller)
    float process (float x, float volume)
    {
        float y = v1.process (x);
        y *= volume;
        return v2.process (y) * 0.5f;
    }

private:
    TriodeStage v1, v2;
};
} // namespace t150
