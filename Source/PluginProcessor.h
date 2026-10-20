#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

#include "synth/Ms20.h"
#include "dsp/Preamp.h"
#include "dsp/ToneStack.h"
#include "dsp/PowerAmp.h"
#include "dsp/CabSim.h"

// T150 Synth: an MS-20 style mono synth played through the T-150 amp and cab.
class T150SynthProcessor : public juce::AudioProcessor
{
public:
    T150SynthProcessor();
    ~T150SynthProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadImpulseResponse (const juce::File& file);
    juce::String getImpulseResponseName() const;

    // For the panel's trigger light
    std::atomic<bool> gateLit { false };

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    static constexpr int oversamplingOrder = 3;   // 2^3 = 8x
    static constexpr int osFactor = 1 << oversamplingOrder;

    ms20::Params readSynthParams() const;
    void handleMidi (const juce::MidiMessage& m);
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> mono;

    ms20::Voice voice;
    t150::Preamp preamp;
    t150::ToneStack toneStack;
    t150::PowerAmp powerAmp;
    t150::Biquad presence;
    t150::CabSim fallbackCab;
    juce::dsp::Convolution cabIR;
    std::atomic<bool> irLoaded { false };

    juce::SmoothedValue<float> volumeSmoothed, masterSmoothed, outputSmoothed;
    double baseSampleRate = 44100.0;
    float lastPresence = -100.0f;
    double feedbackSignal = 0.0;
    double bendSemis = 0.0, modWheel = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (T150SynthProcessor)
};
