#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Identifier irPathId ("irPath");

    // 0..10 dial -> gain, the way an audio-taper pot behaves
    float audioTaper (float dial0to10)
    {
        const float k = juce::jlimit (0.0f, 1.0f, dial0to10 / 10.0f);
        return (std::pow (25.0f, k) - 1.0f) / 24.0f;
    }

    // Knob -> time in seconds, exponential like real envelope pots
    double timeKnob (float k, double minSec, double maxSec)
    {
        return minSec * std::pow (maxSec / minSec, juce::jlimit (0.0f, 10.0f, k) / 10.0);
    }

    // Optional times (delay, hold, glide): fully left = off
    double optionalTimeKnob (float k, double minSec, double maxSec)
    {
        return k < 0.05f ? 0.0 : timeKnob (k, minSec, maxSec);
    }

    // Bipolar -5..+5 knob -> -1..+1 with finer control near the centre
    double bipolar (float v)
    {
        const double x = juce::jlimit (-5.0f, 5.0f, v) / 5.0;
        return x * std::fabs (x);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout T150SynthProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dial = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                            NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    auto bip = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                            NormalisableRange<float> (-5.0f, 5.0f, 0.01f), def));
    };
    auto choice = [&] (const char* id, const char* name, StringArray items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto toggle = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // ---- VCO1 ----
    choice ("v1scale", "VCO1 Scale", { "32'", "16'", "8'", "4'" }, 1);
    choice ("v1wave",  "VCO1 Wave",  { "Triangle", "Saw", "Pulse", "Noise" }, 1);
    dial   ("v1pw",    "VCO1 Pulse Width", 0.0f);
    dial   ("v1pwm",   "VCO1 PWM (MG)", 0.0f);
    // ---- VCO2 ----
    choice ("v2scale", "VCO2 Scale", { "16'", "8'", "4'", "2'" }, 0);
    choice ("v2wave",  "VCO2 Wave",  { "Saw", "Square", "Pulse", "Ring" }, 0);
    bip    ("v2pitch", "VCO2 Pitch", 0.4f);
    // ---- Mixer / master ----
    dial   ("mix1",    "VCO1 Level", 8.0f);
    dial   ("mix2",    "VCO2 Level", 6.0f);
    bip    ("tune",    "Master Tune", 0.0f);
    dial   ("porta",   "Portamento", 0.0f);
    // ---- Frequency modulation ----
    bip    ("fmMg",    "Freq Mod MG", 0.0f);
    dial   ("fmEg1",   "Freq Mod EG1", 0.0f);
    // ---- Filters ----
    dial   ("hpCut",   "HPF Cutoff", 0.0f);
    dial   ("hpPeak",  "HPF Peak", 0.0f);
    bip    ("hpMg",    "HPF Mod MG", 0.0f);
    bip    ("hpEg",    "HPF Mod EG2", 0.0f);
    dial   ("lpCut",   "LPF Cutoff", 5.5f);
    dial   ("lpPeak",  "LPF Peak", 4.0f);
    bip    ("lpMg",    "LPF Mod MG", 0.0f);
    bip    ("lpEg",    "LPF Mod EG2", 2.5f);
    toggle ("rev2",    "Filter Rev2 (late)", false);
    // ---- Envelopes ----
    dial   ("eg1Delay",   "EG1 Delay", 0.0f);
    dial   ("eg1Attack",  "EG1 Attack", 0.0f);
    dial   ("eg1Release", "EG1 Release", 3.0f);
    dial   ("eg2Hold",    "EG2 Hold", 0.0f);
    dial   ("eg2Attack",  "EG2 Attack", 0.0f);
    dial   ("eg2Decay",   "EG2 Decay", 5.0f);
    dial   ("eg2Sustain", "EG2 Sustain", 6.0f);
    dial   ("eg2Release", "EG2 Release", 4.0f);
    // ---- MG ----
    dial   ("mgWave",  "MG Waveform", 5.0f);
    dial   ("mgFreq",  "MG Frequency", 5.0f);
    // ---- Patches ----
    toggle ("sh",      "Patch: S&H to Filters", false);
    dial   ("feedback","Patch: Amp Feedback", 0.0f);
    dial   ("level",   "Synth Level", 5.0f);

    // ---- Amp ----
    toggle ("ampOn",   "Amp On", true);
    dial   ("volume",  "Amp Volume", 5.0f);
    dial   ("bass",    "Amp Bass", 5.0f);
    dial   ("mid",     "Amp Middle", 5.0f);
    dial   ("treble",  "Amp Treble", 5.0f);
    dial   ("master",  "Amp Master", 5.0f);
    dial   ("presence","Amp Presence", 3.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sag", 1 }, "Amp Sag",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.4f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                                                        NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -6.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    toggle ("cabOn",   "Cab", true);

    return { p.begin(), p.end() };
}

T150SynthProcessor::T150SynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool T150SynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void T150SynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseSampleRate = sampleRate;
    const double osRate = sampleRate * osFactor;

    // The synth and the tube stages all run at 8x the sample rate.
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        1, oversamplingOrder, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples ((int) std::round (oversampler->getLatencyInSamples()));
    mono.setSize (1, samplesPerBlock);

    voice.prepare (osRate);
    voice.setParams (readSynthParams());
    preamp.prepare (osRate);
    toneStack.prepare (osRate);
    powerAmp.prepare (osRate);
    fallbackCab.prepare (sampleRate);
    presence.reset();
    lastPresence = -100.0f;
    feedbackSignal = 0.0;

    cabIR.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 1 });

    volumeSmoothed.reset (sampleRate, 0.03);
    masterSmoothed.reset (sampleRate, 0.03);
    outputSmoothed.reset (sampleRate, 0.03);
    volumeSmoothed.setCurrentAndTargetValue (audioTaper (param ("volume")));
    masterSmoothed.setCurrentAndTargetValue (audioTaper (param ("master")));
    outputSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param ("output")));
}

ms20::Params T150SynthProcessor::readSynthParams() const
{
    ms20::Params s;
    s.vco1Oct    = (int) param ("v1scale") - 2;                 // 32' 16' 8' 4'  -> -2 .. +1
    s.vco1Wave   = (int) param ("v1wave");
    s.pulseWidth = 0.5 - 0.48 * param ("v1pw") / 10.0;
    s.pwm        = 0.45 * param ("v1pwm") / 10.0;
    s.vco2Oct    = (int) param ("v2scale") - 1;                 // 16' 8' 4' 2'   -> -1 .. +2
    s.vco2Wave   = (int) param ("v2wave");
    s.vco2Semis  = 12.0 * bipolar (param ("v2pitch"));          // +-1 octave, fine near centre
    s.level1     = param ("mix1") / 10.0;
    s.level2     = param ("mix2") / 10.0;
    s.tuneSemis  = param ("tune") / 5.0;                        // +-100 cents
    s.portaSec   = optionalTimeKnob (param ("porta"), 0.01, 10.0);
    s.fmMgSemis  = 24.0 * bipolar (param ("fmMg"));
    const double e1 = param ("fmEg1") / 10.0;
    s.fmEg1Semis = 48.0 * e1 * e1;

    auto cutoff = [] (float k) { return 50.0 * std::pow (300.0, k / 10.0); };   // 50 Hz .. 15 kHz
    s.hpCutoff = cutoff (param ("hpCut"));
    s.hpRes    = param ("hpPeak") / 10.0;
    s.hpMgOct  = 4.0 * bipolar (param ("hpMg"));
    s.hpEgOct  = 7.0 * bipolar (param ("hpEg"));
    s.lpCutoff = cutoff (param ("lpCut"));
    s.lpRes    = param ("lpPeak") / 10.0;
    s.lpMgOct  = 4.0 * bipolar (param ("lpMg"));
    s.lpEgOct  = 7.0 * bipolar (param ("lpEg"));
    s.rev2     = param ("rev2") > 0.5f;

    s.eg1Delay   = optionalTimeKnob (param ("eg1Delay"), 0.01, 10.0);
    s.eg1Attack  = timeKnob (param ("eg1Attack"),  0.001, 10.0);
    s.eg1Release = timeKnob (param ("eg1Release"), 0.001, 10.0);
    s.eg2Hold    = optionalTimeKnob (param ("eg2Hold"), 0.02, 20.0);
    s.eg2Attack  = timeKnob (param ("eg2Attack"),  0.001, 10.0);
    s.eg2Decay   = timeKnob (param ("eg2Decay"),   0.001, 10.0);
    s.eg2Sustain = param ("eg2Sustain") / 10.0;
    s.eg2Release = timeKnob (param ("eg2Release"), 0.001, 10.0);

    s.mgShape    = param ("mgWave") / 10.0;
    s.mgFreq     = 0.1 * std::pow (200.0, param ("mgFreq") / 10.0);   // 0.1 .. 20 Hz
    s.sampleHold = param ("sh") > 0.5f;
    s.feedback   = 1.2 * param ("feedback") / 10.0;
    s.outLevel   = 0.6 * param ("level") / 10.0;

    s.bendSemis = bendSemis;
    s.modWheel  = modWheel;
    return s;
}

void T150SynthProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                         voice.noteOn (m.getNoteNumber());
    else if (m.isNoteOff())                   voice.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) voice.allNotesOff();
    else if (m.isPitchWheel())
    {
        bendSemis = 2.0 * (m.getPitchWheelValue() - 8192) / 8192.0;   // +-2 semitones
        voice.p.bendSemis = bendSemis;
    }
    else if (m.isControllerOfType (1))
    {
        modWheel = m.getControllerValue() / 127.0;
        voice.p.modWheel = modWheel;
    }
}

void T150SynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // ---- Read controls once per block ----
    voice.setParams (readSynthParams());
    const bool ampOn = param ("ampOn") > 0.5f;
    toneStack.setControls (param ("bass") / 10.0f, param ("mid") / 10.0f, param ("treble") / 10.0f);
    powerAmp.setSag (param ("sag"));
    volumeSmoothed.setTargetValue (audioTaper (param ("volume")));
    masterSmoothed.setTargetValue (audioTaper (param ("master")));
    outputSmoothed.setTargetValue (juce::Decibels::decibelsToGain (param ("output")));

    // ---- Synth + amp, at 8x the sample rate ----
    mono.setSize (1, numSamples, false, false, true);
    mono.clear();
    juce::dsp::AudioBlock<float> monoBlock (mono);
    auto os = oversampler->processSamplesUp (monoBlock);   // gives us an 8x buffer to fill
    float* d = os.getChannelPointer (0);

    auto event = midi.cbegin();
    for (int i = 0; i < numSamples; ++i)
    {
        // MIDI lands on the exact sample it was played
        while (event != midi.cend() && (*event).samplePosition <= i)
        {
            handleMidi ((*event).getMessage());
            ++event;
        }

        const float vol = volumeSmoothed.getNextValue();
        const float mst = masterSmoothed.getNextValue();

        for (int k = 0; k < osFactor; ++k)
        {
            const double s = voice.render (feedbackSignal);
            double y = s;
            if (ampOn)
            {
                y = preamp.process ((float) s, vol);
                y = toneStack.process ((float) y) * mst;
                y = powerAmp.process ((float) y);
            }
            feedbackSignal = y;            // the "amp -> filter" feedback patch
            d[i * osFactor + k] = (float) y;
        }
    }
    for (; event != midi.cend(); ++event)
        handleMidi ((*event).getMessage());

    gateLit = voice.isActive();
    oversampler->processSamplesDown (monoBlock);

    // ---- Presence, cabinet, output ----
    float* m = mono.getWritePointer (0);
    if (ampOn)
    {
        const float pres = param ("presence");
        if (std::abs (pres - lastPresence) > 1.0e-6f)
        {
            lastPresence = pres;
            presence.set (t150::Biquad::Type::highShelf, baseSampleRate, 3000.0, 0.7, (pres - 3.0) * 1.2);
        }
        for (int n = 0; n < numSamples; ++n)
            m[n] = presence.process (m[n]);
    }

    if (param ("cabOn") > 0.5f)
    {
        if (irLoaded.load())
        {
            juce::dsp::ProcessContextReplacing<float> ctx (monoBlock);
            cabIR.process (ctx);
        }
        else
        {
            for (int n = 0; n < numSamples; ++n)
                m[n] = fallbackCab.process (m[n]);
        }
    }

    for (int n = 0; n < numSamples; ++n)
        m[n] *= outputSmoothed.getNextValue();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mono, 0, 0, numSamples);
}

void T150SynthProcessor::loadImpulseResponse (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    cabIR.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes,
                               0, juce::dsp::Convolution::Normalise::yes);
    apvts.state.setProperty (irPathId, file.getFullPathName(), nullptr);
    irLoaded = true;
}

juce::String T150SynthProcessor::getImpulseResponseName() const
{
    const auto path = apvts.state.getProperty (irPathId).toString();
    return path.isEmpty() ? juce::String ("Built-in cab") : juce::File (path).getFileNameWithoutExtension();
}

void T150SynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void T150SynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            const auto path = apvts.state.getProperty (irPathId).toString();
            if (path.isNotEmpty())
                loadImpulseResponse (juce::File (path));
        }
    }
}

juce::AudioProcessorEditor* T150SynthProcessor::createEditor()
{
    return new T150SynthEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new T150SynthProcessor();
}
