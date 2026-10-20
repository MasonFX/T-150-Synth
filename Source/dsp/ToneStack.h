#pragma once
// Fender/Bassman 5F6A tone stack (the "FMV" stack), which the 1st-gen
// Sunn Model T uses.
//
// How it works:
//   1. The real circuit is 3 capacitors + 4 resistors (3 of them are the
//      Bass/Mid/Treble pots). Its frequency response is a 3rd-order
//      transfer function H(s) = (b1 s + b2 s^2 + b3 s^3) / (1 + a1 s + a2 s^2 + a3 s^3).
//   2. The coefficients below come from solving that circuit exactly
//      (nodal analysis), so the knobs interact just like the real amp.
//   3. The bilinear transform turns H(s) into a digital filter.
//
// Approach follows Yeh & Smith, "Discretization of the '59 Fender Bassman
// Tone Stack" (DAFx 2006).
//
// Part values are the Bassman 5F6A's. CHECK THEM against your Model T
// schematic and change them here if they differ.
#include <cmath>

namespace t150
{
class ToneStack
{
public:
    // Component values (ohms / farads)
    double R1 = 250e3;  // treble pot
    double R2 = 1e6;    // bass pot
    double R3 = 25e3;   // mid pot
    double R4 = 56e3;   // "slope" resistor
    double C1 = 250e-12;// treble cap
    double C2 = 20e-9;  // bass cap
    double C3 = 20e-9;  // mid cap

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        reset();
        updateCoefficients();
    }

    void reset() { z[0] = z[1] = z[2] = 0.0; }

    // Knob positions 0..1 (like the dial, 0 = fully left, 1 = fully right)
    void setControls (float bassKnob, float midKnob, float trebleKnob)
    {
        if (std::fabs (bassKnob - lastB) < 1e-6f && std::fabs (midKnob - lastM) < 1e-6f
            && std::fabs (trebleKnob - lastT) < 1e-6f)
            return;
        lastB = bassKnob; lastM = midKnob; lastT = trebleKnob;

        // Bass and treble pots on these amps are "audio" (log) taper,
        // mid is linear. This maps the dial to the actual resistance fraction.
        l = audioTaper (bassKnob);
        m = clamp01 (midKnob);
        t = audioTaper (trebleKnob);
        updateCoefficients();
    }

    float process (float in)
    {
        // Transposed direct form II, in double precision for stability.
        const double x = in;
        const double y = B[0] * x + z[0];
        z[0] = B[1] * x - A[1] * y + z[1];
        z[1] = B[2] * x - A[2] * y + z[2];
        z[2] = B[3] * x - A[3] * y;
        return (float) y;
    }

private:
    static double clamp01 (double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    static double audioTaper (double knob)
    {
        knob = clamp01 (knob);
        return (std::pow (25.0, knob) - 1.0) / 24.0; // ~17% at noon
    }

    void updateCoefficients()
    {
        // Keep pots from hitting exactly 0 ohms (avoids a degenerate filter).
        const double tt = t < 1e-4 ? 1e-4 : (t > 1.0 - 1e-4 ? 1.0 - 1e-4 : t);
        const double mm = m < 1e-4 ? 1e-4 : m;
        const double ll = l < 1e-4 ? 1e-4 : l;

        // --- Analog coefficients, solved from the circuit ---
        const double b1 = tt*C1*R1 + ll*(C1*R2 + C2*R2) + mm*(C1*R3 + C2*R3 + C3*R3);

        const double b2 = tt*(C1*C2*R1*R4 + C1*C3*R1*R4)
                        + ll*(C1*C2*R1*R2 + C1*C2*R2*R4 + C1*C3*R2*R4)
                        + mm*(C1*C2*R1*R3 + C1*C2*R3*R4 + C1*C3*R1*R3 + C1*C3*R3*R4)
                        + ll*mm*(C1*C3*R2*R3 + C2*C3*R2*R3);

        const double b3 = ll*tt*(C1*C2*C3*R1*R2*R4)
                        + ll*mm*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4);

        const double a1 = C1*R1 + C2*R4 + C3*R4
                        + ll*(C1*R2 + C2*R2) + mm*(C1*R3 + C2*R3 + C3*R3);

        const double a2 = C1*C2*R1*R4 + C1*C3*R1*R4
                        + ll*(C1*C2*R1*R2 + C1*C2*R2*R4 + C1*C3*R2*R4 + C2*C3*R2*R4)
                        + mm*(C1*C2*R1*R3 + C1*C2*R3*R4 + C1*C3*R1*R3 + C1*C3*R3*R4)
                        + ll*mm*(C1*C3*R2*R3 + C2*C3*R2*R3);

        const double a3 = ll*(C1*C2*C3*R1*R2*R4)
                        + ll*mm*(C1*C2*C3*R1*R2*R3 + C1*C2*C3*R2*R3*R4);
        const double a0 = 1.0, b0 = 0.0;

        // --- Bilinear transform: s -> c * (1 - z^-1) / (1 + z^-1) ---
        const double c = 2.0 * fs, c2 = c * c, c3 = c2 * c;

        const double Bz0 =  b0 + b1*c + b2*c2 + b3*c3;
        const double Bz1 = 3*b0 + b1*c - b2*c2 - 3*b3*c3;
        const double Bz2 = 3*b0 - b1*c - b2*c2 + 3*b3*c3;
        const double Bz3 =  b0 - b1*c + b2*c2 - b3*c3;

        const double Az0 =  a0 + a1*c + a2*c2 + a3*c3;
        const double Az1 = 3*a0 + a1*c - a2*c2 - 3*a3*c3;
        const double Az2 = 3*a0 - a1*c - a2*c2 + 3*a3*c3;
        const double Az3 =  a0 - a1*c + a2*c2 - a3*c3;

        B[0] = Bz0 / Az0; B[1] = Bz1 / Az0; B[2] = Bz2 / Az0; B[3] = Bz3 / Az0;
        A[0] = 1.0;       A[1] = Az1 / Az0; A[2] = Az2 / Az0; A[3] = Az3 / Az0;
    }

    double fs = 48000.0;
    double t = 0.17, m = 0.5, l = 0.17;
    float lastB = -1.f, lastM = -1.f, lastT = -1.f;
    double B[4] {}, A[4] {}, z[3] {};
};
} // namespace t150
