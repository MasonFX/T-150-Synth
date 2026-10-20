#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Draws the knobs, toggle switches and buttons to look like vintage hardware.
class AmpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AmpLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
};

class T150SynthEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit T150SynthEditor (T150SynthProcessor&);
    ~T150SynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // One control on the panel: a knob (0-10, bipolar or a switch-style
    // selector) or a toggle switch.
    struct Ctl
    {
        enum Kind { dial, bipolar, selector, toggle } kind = dial;
        juce::String id, name, onLabel, offLabel;
        juce::StringArray positions;   // selector labels around the knob
        juce::Slider slider;
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttach;
        juce::Rectangle<int> cell;
    };

    struct Group
    {
        Group (juce::String t, std::vector<Ctl*> i) : title (std::move (t)), items (std::move (i)) {}
        juce::String title;
        std::vector<Ctl*> items;
        juce::Rectangle<int> box;
    };

    Ctl* addDial     (const juce::String& id, const juce::String& name);
    Ctl* addBipolar  (const juce::String& id, const juce::String& name);
    Ctl* addSelector (const juce::String& id, const juce::String& name, juce::StringArray positions);
    Ctl* addToggle   (const juce::String& id, const juce::String& name, const juce::String& on, const juce::String& off);
    Ctl* makeKnob    (Ctl::Kind kind, const juce::String& id, const juce::String& name);

    void layoutRow (std::vector<Group>& row, juce::Rectangle<int> rowArea, bool drawBoxes);
    void drawControlArt (juce::Graphics&, const Ctl&, juce::Colour ink, juce::Colour inkDim) const;
    void chooseIR();
    void renderBackground();
    void timerCallback() override;

    T150SynthProcessor& processor;
    AmpLookAndFeel look;

    std::vector<std::unique_ptr<Ctl>> controls;
    std::vector<std::vector<Group>> synthRows;
    std::vector<Group> ampRow;
    Ctl* ampOnCtl = nullptr;
    Ctl* cabCtl = nullptr;

    juce::TextButton loadIRButton { "LOAD IR" };
    juce::Label irLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Rectangle<int> synthPanel, ampPanel, trigLamp, powerLamp;
    juce::Image background;
    bool lastGate = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (T150SynthEditor)
};
