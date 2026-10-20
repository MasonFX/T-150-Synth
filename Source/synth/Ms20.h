#pragma once
// MS-20 style monophonic synth voice.
//
//   VCO1 ─┐
//         ├─ mixer ─(+ feedback patch)─ HPF ─ LPF ─ VCA ─ out
//   VCO2 ─┘
//
//   EG1 (Delay / Attack / Release) ── pitch sweep ("EG1/EXT" knob)
//   EG2 (Hold / Attack / Decay / Sustain / Release) ── filters + VCA
//   MG  (shape-morphing LFO) ── pitch ("MG/T.EXT") and filters
//   S&H (noise sampled at the MG rate) ── optional filter modulation patch
//
// Two filter revisions, like the real thing:
//   Rev 1 (early): Korg35-style filters. Resonance goes through a clipping
//                  stage, so it screams and growls when pushed.
//   Rev 2 (late):  OTA-style filters. Smoother, rounder, more controlled.
//
// Plain C++ with no JUCE, so it can be tested on its own.
// Runs at the plugin's oversampled rate (8x), which keeps the oscillators and
// filters clean at high pitches and cutoffs.
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace ms20
{
constexpr double kPi = 3.14159265358979323846;

inline double clampd (double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

// --- Helpers ---------------------------------------------------------------

// PolyBLEP: smooths the jump in a saw/pulse wave so it doesn't alias.
inline double polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt;               return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt;    return t * t + t + t + 1.0; }
    return 0.0;
}

struct Noise
{
    uint32_t s = 0x1234567u;
    double white() { s = s * 1664525u + 1013904223u; return (double) (s >> 8) * (2.0 / 16777216.0) - 1.0; }
};

// Smoothly follows a target value, so knob moves don't click.
struct Smooth
{
    double v = 0.0, target = 0.0, coef = 0.001;
    void setTime (double seconds, double fs) { coef = 1.0 - std::exp (-1.0 / (seconds * fs)); }
    void snap (double x) { v = target = x; }
    double next() { v += (target - v) * coef; return v; }
};

// --- Oscillators -----------------------------------------------------------

struct Oscillator
{
    double phase = 0.0;

    void advance (double dt) { phase += dt; if (phase >= 1.0) phase -= 1.0; }

    double saw (double dt) const { return 2.0 * phase - 1.0 - polyBlep (phase, dt); }

    double pulse (double dt, double width) const   // +1 for the first `width` of the cycle
    {
        double v = phase < width ? 1.0 : -1.0;
        v += polyBlep (phase, dt);
        double t = phase - width;
        if (t < 0.0) t += 1.0;
        v -= polyBlep (t, dt);
        return v;
    }

    double triangle() const { return phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase; }
};

// --- Filters ---------------------------------------------------------------

// One-pole "zero delay feedback" building block (Zavalishin / Pirkle).
struct OnePoleZdf
{
    double G = 0.0, z = 0.0, beta = 0.0;
    void reset() { z = 0.0; }
    double lp (double x) { const double v = (x - z) * G; const double y = v + z; z = y + v; return y; }
    double hp (double x) { const double v = (x - z) * G; const double l = v + z; z = l + v; return x - l; }
    double feedbackOut() const { return z * beta; }
};

// Korg35 low-pass (early MS-20). Structure after Will Pirkle's virtual-analog
// Korg35 model: two one-pole stages with a resonant feedback loop, and a
// saturating stage in the loop that clips the resonance like the real diodes.
struct Korg35Lpf
{
    OnePoleZdf lpf1, lpf2, hpf1;
    double K = 0.01, alpha0 = 1.0, drive = 1.0;

    void reset() { lpf1.reset(); lpf2.reset(); hpf1.reset(); }

    void set (double cutoff, double fs, double k)
    {
        const double g = std::tan (kPi * cutoff / fs);
        const double G = g / (1.0 + g);
        lpf1.G = lpf2.G = hpf1.G = G;
        K = k;
        lpf2.beta = (K - K * G) / (1.0 + g);
        hpf1.beta = -1.0 / (1.0 + g);
        alpha0 = 1.0 / (1.0 - K * G + K * G * G);
    }

    double process (double x)
    {
        const double y1 = lpf1.lp (x);
        // The "diodes": clip only the resonance loop, so the resonance growls
        // and screams while the rest of the sound passes through at full level.
        const double s35 = std::tanh (drive * (hpf1.feedbackOut() + lpf2.feedbackOut())) / drive;
        const double u = alpha0 * (y1 + s35);
        const double y = K * lpf2.lp (u);
        hpf1.hp (y);
        return y / K;
    }
};

// Korg35 high-pass (early MS-20), same idea.
struct Korg35Hpf
{
    OnePoleZdf hpf1, hpf2, lpf1;
    double K = 0.01, alpha0 = 1.0, drive = 1.0;

    void reset() { hpf1.reset(); hpf2.reset(); lpf1.reset(); }

    void set (double cutoff, double fs, double k)
    {
        const double g = std::tan (kPi * cutoff / fs);
        const double G = g / (1.0 + g);
        hpf1.G = hpf2.G = lpf1.G = G;
        K = k;
        hpf2.beta = -G / (1.0 + g);
        lpf1.beta = 1.0 / (1.0 + g);
        alpha0 = 1.0 / (1.0 - K * G + K * G * G);
    }

    double process (double x)
    {
        const double y1 = hpf1.hp (x);
        const double s35 = std::tanh (drive * (hpf2.feedbackOut() + lpf1.feedbackOut())) / drive;
        const double u = alpha0 * (y1 + s35);
        const double y = K * u;
        lpf1.lp (hpf2.hp (y));
        return y / K;
    }
};

// OTA-style 2-pole filter (late MS-20): state-variable filter with soft
// saturation at the input and on the resonance path. Smoother than Korg35.
struct OtaFilter
{
    double ic1 = 0.0, ic2 = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0, k = 2.0;
    bool highPass = false;

    void reset() { ic1 = ic2 = 0.0; }

    void set (double cutoff, double fs, double res01)
    {
        const double g = std::tan (kPi * cutoff / fs);
        k = 2.0 - 2.05 * std::pow (res01, 1.4);      // gentler resonance; just past 0 at max for soft self-oscillation
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double process (double x)
    {
        const double v0 = std::tanh (0.8 * x) / 0.8; // OTA input stage
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        ic1 = ic1 / (1.0 + 0.08 * ic1 * ic1);        // soft limit keeps self-oscillation round and bounded
        return highPass ? (v0 - k * v1 - v2) : v2;
    }
};

// --- Envelopes and modulation ----------------------------------------------

inline double rcCoef (double seconds, double fs) { return 1.0 - std::exp (-1.0 / (std::fmax (seconds, 1.0e-4) * fs)); }

// EG1: Delay -> Attack -> (full level while held) -> Release.
struct Eg1
{
    enum Stage { idle, delay, attack, sustain, release } stage = idle;
    double v = 0.0, delayLeft = 0.0;

    void gateOn (double delaySec) { delayLeft = delaySec; stage = delaySec > 0.0 ? delay : attack; }
    void gateOff() { if (stage != idle) stage = release; }

    double next (double fs, double attackSec, double releaseSec)
    {
        switch (stage)
        {
            case delay:   delayLeft -= 1.0 / fs; if (delayLeft <= 0.0) stage = attack; break;
            case attack:  v += (1.3 - v) * rcCoef (attackSec / 1.5, fs); if (v >= 1.0) { v = 1.0; stage = sustain; } break;
            case sustain: v = 1.0; break;
            case release: v += (-0.01 - v) * rcCoef (releaseSec / 4.0, fs); if (v <= 0.0) { v = 0.0; stage = idle; } break;
            case idle:    break;
        }
        return v;
    }
};

// EG2: Hold -> Attack -> Decay -> Sustain -> Release.
// Hold stretches a short key press so the envelope can play out (MS-20 style).
struct Eg2
{
    enum Stage { idle, attack, decay, sustain, release } stage = idle;
    double v = 0.0, holdLeft = 0.0;
    bool keyDown = false;

    void gateOn (double holdSec) { keyDown = true; holdLeft = holdSec; stage = attack; }
    void gateOff() { keyDown = false; }

    double next (double fs, double a, double d, double s, double r)
    {
        if (holdLeft > 0.0) holdLeft -= 1.0 / fs;
        const bool gate = keyDown || holdLeft > 0.0;
        if (! gate && stage != idle && stage != release) stage = release;

        switch (stage)
        {
            case attack:  v += (1.3 - v) * rcCoef (a / 1.5, fs); if (v >= 1.0) { v = 1.0; stage = decay; } break;
            case decay:   v += (s - v) * rcCoef (d / 4.0, fs); if (std::fabs (v - s) < 1.0e-4) stage = sustain; break;
            case sustain: v = s; break;
            case release: v += (-0.01 - v) * rcCoef (r / 4.0, fs); if (v <= 0.0) { v = 0.0; stage = idle; } break;
            case idle:    break;
        }
        return v;
    }
};

// MG: one knob morphs the wave from falling saw -> triangle -> rising saw
// (and the matching pulse from wide to narrow). Also drives the S&H clock.
struct Mg
{
    double phase = 0.0, sh = 0.0;
    Noise noise;

    // returns the triangle/saw output (-1..1); updates the S&H value on each cycle
    double next (double freq, double fs, double shape)
    {
        phase += freq / fs;
        if (phase >= 1.0) { phase -= 1.0; sh = noise.white(); }
        const double w = clampd (shape, 0.001, 0.999);
        return phase < w ? -1.0 + 2.0 * phase / w : 1.0 - 2.0 * (phase - w) / (1.0 - w);
    }
};

// --- Parameters (already in real-world units) ------------------------------

struct Params
{
    // VCO1: wave 0=triangle 1=saw 2=pulse 3=noise; octave -2..+1 (32'..4')
    int    vco1Wave = 1, vco1Oct = -1;
    double pulseWidth = 0.5;      // 0.5 = square ... 0.02 = very narrow
    double pwm = 0.0;             // MG -> pulse width amount (0..0.45)

    // VCO2: wave 0=saw 1=square 2=narrow pulse 3=ring; octave -1..+2 (16'..2')
    int    vco2Wave = 0, vco2Oct = -1;
    double vco2Semis = 0.0;       // the VCO2 PITCH knob

    double level1 = 0.8, level2 = 0.0;
    double tuneSemis = 0.0, portaSec = 0.0;

    double fmMgSemis = 0.0;       // MG/T.EXT -> pitch (bipolar)
    double fmEg1Semis = 0.0;      // EG1/EXT  -> pitch sweep from below

    double hpCutoff = 50.0, hpRes = 0.0, hpMgOct = 0.0, hpEgOct = 0.0;
    double lpCutoff = 2000.0, lpRes = 0.3, lpMgOct = 0.0, lpEgOct = 0.0;
    bool   rev2 = false;

    double eg1Delay = 0.0, eg1Attack = 0.001, eg1Release = 0.05;
    double eg2Hold = 0.0, eg2Attack = 0.002, eg2Decay = 0.3, eg2Sustain = 0.7, eg2Release = 0.2;

    double mgShape = 0.5, mgFreq = 2.0;
    bool   sampleHold = false;    // filter MG mod uses S&H instead of MG
    double feedback = 0.0;        // amp output -> filter input
    double outLevel = 0.3;

    double bendSemis = 0.0, modWheel = 0.0;
};

// --- Voice ------------------------------------------------------------------

class Voice
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto* s : { &sLevel1, &sLevel2, &sHpCut, &sLpCut, &sHpRes, &sLpRes, &sFeedback, &sOut })
            s->setTime (0.008, fs);
        dcState = dcPrev = 0.0;
        dcCoef = std::exp (-2.0 * kPi * 8.0 / fs);
        reset();
    }

    void reset()
    {
        k35l.reset(); k35h.reset(); otaL.reset(); otaH.reset();
        eg1 = Eg1(); eg2 = Eg2();
        for (auto& h : held) h = false;
        heldCount = 0;
        first = true;
    }

    void setParams (const Params& np)
    {
        p = np;
        sLevel1.target = p.level1;    sLevel2.target = p.level2;
        sHpCut.target = std::log2 (p.hpCutoff);  sLpCut.target = std::log2 (p.lpCutoff);
        sHpRes.target = p.hpRes;      sLpRes.target = p.lpRes;
        sFeedback.target = p.feedback; sOut.target = p.outLevel;
        if (first)
        {
            for (auto* s : { &sLevel1, &sLevel2, &sHpCut, &sLpCut, &sHpRes, &sLpRes, &sFeedback, &sOut })
                s->snap (s->target);
            first = false;
        }
    }

    // ---- Keyboard: one note at a time, lowest held key wins (like the MS-20) ----
    void noteOn (int note)
    {
        if (note < 0 || note > 127) return;
        const bool wasEmpty = (heldCount == 0);
        if (! held[note]) { held[note] = true; ++heldCount; }
        targetSemis = (double) lowestHeld();
        if (wasEmpty)
        {
            if (p.portaSec <= 0.0 || ! hasPlayed) currentSemis = targetSemis;
            eg1.gateOn (p.eg1Delay);
            eg2.gateOn (p.eg2Hold);
            hasPlayed = true;
        }
    }

    void noteOff (int note)
    {
        if (note < 0 || note > 127 || ! held[note]) return;
        held[note] = false;
        --heldCount;
        if (heldCount == 0) { eg1.gateOff(); eg2.gateOff(); }
        else                targetSemis = (double) lowestHeld();   // fall back to the remaining key, no retrigger
    }

    void allNotesOff()
    {
        for (auto& h : held) h = false;
        heldCount = 0;
        eg1.gateOff(); eg2.gateOff();
    }

    bool isActive() const { return eg2.stage != Eg2::idle || heldCount > 0; }

    // Render one sample. `feedbackIn` is the patched signal coming back from the amp.
    double render (double feedbackIn)
    {
        // Glide
        if (p.portaSec > 0.0) currentSemis += (targetSemis - currentSemis) * rcCoef (p.portaSec / 3.0, fs);
        else                  currentSemis = targetSemis;

        // Modulators
        const double mg  = mgen.next (p.mgFreq, fs, p.mgShape);
        const double e1  = eg1.next (fs, p.eg1Attack, p.eg1Release);
        const double e2  = eg2.next (fs, p.eg2Attack, p.eg2Decay, p.eg2Sustain, p.eg2Release);
        const double filtMod = p.sampleHold ? mgen.sh : mg;

        // Pitch
        const double fm = p.fmMgSemis * mg + p.modWheel * 0.6 * mg - p.fmEg1Semis * (1.0 - e1);
        const double base = currentSemis + p.tuneSemis + p.bendSemis + fm;

        const double f1 = noteHz (base + 12.0 * p.vco1Oct);
        const double f2 = noteHz (base + 12.0 * p.vco2Oct + p.vco2Semis);
        const double dt1 = clampd (f1 / fs, 0.0, 0.45), dt2 = clampd (f2 / fs, 0.0, 0.45);

        // VCO1
        const double width = clampd (p.pulseWidth - p.pwm * 0.5 * (mg + 1.0), 0.02, 0.5);
        double v1 = 0.0;
        switch (p.vco1Wave)
        {
            case 0:  v1 = osc1.triangle(); break;
            case 1:  v1 = osc1.saw (dt1); break;
            case 2:  v1 = osc1.pulse (dt1, width); break;
            default: v1 = noise.white(); break;
        }

        // VCO2 ("ring" = VCO1 pulse combined with VCO2 square, as on the MS-20)
        double v2 = 0.0;
        switch (p.vco2Wave)
        {
            case 0:  v2 = osc2.saw (dt2); break;
            case 1:  v2 = osc2.pulse (dt2, 0.5); break;
            case 2:  v2 = osc2.pulse (dt2, 0.12); break;
            default: v2 = osc1.pulse (dt1, width) * osc2.pulse (dt2, 0.5); break;
        }
        osc1.advance (dt1);
        osc2.advance (dt2);

        // Mixer + feedback patch
        double x = 0.6 * (sLevel1.next() * v1 + sLevel2.next() * v2) + sFeedback.next() * feedbackIn;
        x += 1.0e-5 * analogNoise.white();   // tiny circuit noise: lets a maxed-out filter start singing on its own

        // Filters: HPF then LPF
        const double nyq = 0.42 * fs;
        const double hpHz = clampd (std::exp2 (sHpCut.next() + p.hpMgOct * filtMod + p.hpEgOct * (e2 - p.eg2Sustain)), 20.0, std::fmin (20000.0, nyq));
        const double lpHz = clampd (std::exp2 (sLpCut.next() + p.lpMgOct * filtMod + p.lpEgOct * (e2 - p.eg2Sustain)), 20.0, std::fmin (20000.0, nyq));
        const double hr = sHpRes.next(), lr = sLpRes.next();

        if (! p.rev2)
        {
            k35h.set (hpHz, fs, 0.02 + 2.25 * hr);
            k35l.set (lpHz, fs, 0.02 + 2.25 * lr);
            x = k35l.process (k35h.process (x));
        }
        else
        {
            otaH.highPass = true;
            otaH.set (hpHz, fs, hr);
            otaL.set (lpHz, fs, lr);
            x = otaL.process (otaH.process (x));
        }

        // VCA (driven by EG2), output level, DC blocker
        x *= e2 * sOut.next();
        const double y = x - dcPrev + dcCoef * dcState;
        dcPrev = x; dcState = y;
        return y;
    }

    static double noteHz (double semis) { return 440.0 * std::exp2 ((semis - 69.0) / 12.0); }

    // exposed for tests
    Params p;
    double currentSemis = 60.0, targetSemis = 60.0;

private:
    int lowestHeld() const
    {
        for (int i = 0; i < 128; ++i) if (held[i]) return i;
        return (int) targetSemis;
    }

    double fs = 48000.0;
    Oscillator osc1, osc2;
    Noise noise, analogNoise;
    Korg35Lpf k35l;
    Korg35Hpf k35h;
    OtaFilter otaL, otaH;
    Eg1 eg1;
    Eg2 eg2;
    Mg mgen;
    Smooth sLevel1, sLevel2, sHpCut, sLpCut, sHpRes, sLpRes, sFeedback, sOut;
    double dcState = 0.0, dcPrev = 0.0, dcCoef = 0.999;
    bool held[128] {};
    int heldCount = 0;
    bool first = true, hasPlayed = false;
};
} // namespace ms20
