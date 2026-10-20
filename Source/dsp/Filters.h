#pragma once
// Small, dependency-free filter building blocks used by the amp stages.
#include <cmath>

namespace t150
{
constexpr double kPi = 3.14159265358979323846;

// One-pole high-pass. Models the coupling capacitors between tube stages:
// they block DC and roll off the very lowest frequencies.
struct OnePoleHP
{
    void prepare (double sampleRate, double cutoffHz)
    {
        const double x = std::exp (-2.0 * kPi * cutoffHz / sampleRate);
        a = x;
        reset();
    }
    void reset() { x1 = y1 = 0.0; }
    float process (float in)
    {
        const double y = a * (y1 + in - x1);
        x1 = in;
        y1 = y;
        return (float) y;
    }
    double a = 0.0, x1 = 0.0, y1 = 0.0;
};

// One-pole low-pass. Models the natural high-frequency roll-off of a tube
// stage (the "Miller" capacitance) so the distortion doesn't get fizzy.
struct OnePoleLP
{
    void prepare (double sampleRate, double cutoffHz)
    {
        b = 1.0 - std::exp (-2.0 * kPi * cutoffHz / sampleRate);
        reset();
    }
    void reset() { y1 = 0.0; }
    float process (float in)
    {
        y1 += b * (in - y1);
        return (float) y1;
    }
    double b = 0.0, y1 = 0.0;
};

// Standard "Audio EQ Cookbook" biquad (Robert Bristow-Johnson).
// Used for the presence shelf and the built-in fallback cabinet.
struct Biquad
{
    enum class Type { lowPass, highPass, peak, highShelf };

    void set (Type type, double sampleRate, double freq, double q, double gainDb = 0.0)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * freq / sampleRate;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw / (2.0 * q);
        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;

        switch (type)
        {
            case Type::lowPass:
                B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2;
                A0 = 1 + alpha;    A1 = -2 * cw; A2 = 1 - alpha;
                break;
            case Type::highPass:
                B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2;
                A0 = 1 + alpha;    A1 = -2 * cw;   A2 = 1 - alpha;
                break;
            case Type::peak:
                B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A;
                A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A;
                break;
            case Type::highShelf:
            {
                const double sq = 2.0 * std::sqrt (A) * alpha;
                B0 = A * ((A + 1) + (A - 1) * cw + sq);
                B1 = -2 * A * ((A - 1) + (A + 1) * cw);
                B2 = A * ((A + 1) + (A - 1) * cw - sq);
                A0 = (A + 1) - (A - 1) * cw + sq;
                A1 = 2 * ((A - 1) - (A + 1) * cw);
                A2 = (A + 1) - (A - 1) * cw - sq;
                break;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    void reset() { z1 = z2 = 0.0; }
    float process (float in)
    {
        const double y = b0 * in + z1;
        z1 = b1 * in - a1 * y + z2;
        z2 = b2 * in - a2 * y;
        return (float) y;
    }
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};
} // namespace t150
