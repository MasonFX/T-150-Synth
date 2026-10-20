#pragma once
// Built-in fallback cabinet, used until you load a real impulse response.
// It's a rough sketch of a closed-back 4x12: low thump, cut lows below it,
// a little upper-mid "cone" bump, and a steep top-end roll-off.
// A real IR will sound much better; this just keeps the amp usable.
#include "Filters.h"

namespace t150
{
class CabSim
{
public:
    void prepare (double sr)
    {
        using T = Biquad::Type;
        hp.set   (T::highPass, sr, 75.0,   0.8);
        thump.set(T::peak,     sr, 110.0,  1.2,  3.5);
        cone.set (T::peak,     sr, 2400.0, 1.5,  3.0);
        lp1.set  (T::lowPass,  sr, 4800.0, 0.7);
        lp2.set  (T::lowPass,  sr, 6500.0, 0.7);
        reset();
    }
    void reset() { hp.reset(); thump.reset(); cone.reset(); lp1.reset(); lp2.reset(); }

    float process (float x)
    {
        return lp2.process (lp1.process (cone.process (thump.process (hp.process (x)))));
    }

private:
    Biquad hp, thump, cone, lp1, lp2;
};
} // namespace t150
