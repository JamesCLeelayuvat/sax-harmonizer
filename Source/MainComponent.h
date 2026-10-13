#pragma once

#include <JuceHeader.h>
#include "DSP/HarmonizerEngine.h"
#include "GUI/NoteKeyboard.h"
#include "GUI/LevelMeter.h"

//==============================================================================
/*
    The app window's content: opens the audio device (mono mic in, stereo out),
    forwards the audio callbacks to HarmonizerEngine, and hosts the GUI.

    Around the engine it handles only routing and monitoring: input/output
    metering, the master gain, and copying the engine's mono result to both
    speakers. All harmonizing happens inside HarmonizerEngine.
*/
class MainComponent  : public juce::AudioAppComponent,
                       private juce::Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    void timerCallback() override;
    void showAudioSettings();
    void updateVoicesLabel (float inputFrequency);
    void setUpGainSlider (juce::Slider& slider, juce::Label& label, const juce::String& name,
                          double initialDb, std::function<void (float)> onGainChange);

    HarmonizerEngine engine;

    // Shared with the audio thread
    std::atomic<float> masterGain { 1.0f };
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

    // Audio thread only
    juce::SmoothedValue<float> smoothedMasterGain;

    // GUI
    juce::Label titleLabel, pitchLabel, voicesLabel;
    juce::TextButton settingsButton { "Audio Settings..." };
    juce::TextButton clearButton { "Clear" };
    juce::ComboBox transpositionBox;
    juce::Label transpositionLabel;
    NoteKeyboard keyboard { engine.getNoteController() };
    juce::Slider dryGainSlider, harmonyGainSlider, masterGainSlider;
    juce::Label dryGainLabel, harmonyGainLabel, masterGainLabel;
    LevelMeter inputMeter, outputMeter;
    juce::Label inputMeterLabel, outputMeterLabel;

    juce::ApplicationProperties appProperties;
    juce::Component::SafePointer<juce::DialogWindow> settingsWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
