#include "MainComponent.h"
#include "Utils/TuningUtils.h"

namespace
{
    constexpr int numInputChannels = 1; // mono mic
    constexpr int numOutputChannels = 2;
    constexpr float minGainDb = -60.0f; // slider bottom = silence
    const char* const deviceStateKey = "audioDeviceState";

    // Raises target to value if value is bigger. Lets the audio thread report a
    // peak that the GUI timer later reads and resets.
    void storeMax (std::atomic<float>& target, float value)
    {
        auto current = target.load();
        while (value > current && ! target.compare_exchange_weak (current, value))
        {
        }
    }

    juce::String describePitch (float hz)
    {
        if (hz <= 0.0f)
            return "--";

        const auto midi = TuningUtils::frequencyToMidiNote (hz);
        const auto nearest = juce::roundToInt (midi);
        const auto cents = juce::roundToInt ((midi - (float) nearest) * 100.0f);

        return juce::MidiMessage::getMidiNoteName (nearest, true, true, 4)
             + "   " + juce::String (hz, 1) + " Hz   "
             + (cents >= 0 ? "+" : "") + juce::String (cents) + " cents";
    }
}

//==============================================================================
MainComponent::MainComponent()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "sax-harmonizer";
    options.folderName = "sax-harmonizer";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);

    titleLabel.setText ("Sax Harmonizer", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    pitchLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    pitchLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (pitchLabel);

    settingsButton.onClick = [this] { showAudioSettings(); };
    addAndMakeVisible (settingsButton);

    keyboard.onSelectionChanged = [this] { updateVoicesLabel (engine.getParameters().detectedFrequency); };
    addAndMakeVisible (keyboard);

    // Combo box item IDs are Transposition + 1 (JUCE reserves ID 0 for "nothing selected")
    transpositionBox.addItem ("Concert (C)", (int) NoteController::Transposition::concert + 1);
    transpositionBox.addItem ("Bb (soprano)", (int) NoteController::Transposition::bFlat + 1);
    transpositionBox.addItem ("Eb (alto)", (int) NoteController::Transposition::eFlat + 1);
    transpositionBox.onChange = [this]
    {
        const auto transposition = (NoteController::Transposition) (transpositionBox.getSelectedId() - 1);
        engine.getNoteController().setTransposition (transposition);
        keyboard.repaint();
        updateVoicesLabel (engine.getParameters().detectedFrequency);
    };
    transpositionBox.setSelectedId ((int) engine.getNoteController().getTransposition() + 1, juce::dontSendNotification);
    addAndMakeVisible (transpositionBox);

    transpositionLabel.setText ("Keyboard in", juce::dontSendNotification);
    transpositionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (transpositionLabel);

    voicesLabel.setFont (juce::FontOptions (16.0f));
    addAndMakeVisible (voicesLabel);

    clearButton.onClick = [this]
    {
        engine.getNoteController().clear();
        keyboard.repaint();
        updateVoicesLabel (engine.getParameters().detectedFrequency);
    };
    addAndMakeVisible (clearButton);

    auto& parameters = engine.getParameters();
    setUpGainSlider (dryGainSlider, dryGainLabel, "Dry", 0.0, [&parameters] (float g) { parameters.dryGain = g; });
    setUpGainSlider (harmonyGainSlider, harmonyGainLabel, "Harmony", 0.0, [&parameters] (float g) { parameters.harmonyGain = g; });
    setUpGainSlider (masterGainSlider, masterGainLabel, "Master", 0.0, [this] (float g) { masterGain = g; });

    for (auto* label : { &inputMeterLabel, &outputMeterLabel })
        addAndMakeVisible (label);
    inputMeterLabel.setText ("Input", juce::dontSendNotification);
    outputMeterLabel.setText ("Output", juce::dontSendNotification);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    updateVoicesLabel (0.0f);
    pitchLabel.setText (describePitch (0.0f), juce::dontSendNotification);

    setSize (900, 560);

    // Restore the device (mic, speakers, buffer size) chosen last time, if any
    auto savedDeviceState = appProperties.getUserSettings()->getXmlValue (deviceStateKey);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? numInputChannels : 0, numOutputChannels); });
    }
    else
    {
        setAudioChannels (numInputChannels, numOutputChannels, savedDeviceState.get());
    }

    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    settingsWindow.deleteAndZero(); // it points at our deviceManager

    if (auto deviceState = deviceManager.createStateXml())
    {
        appProperties.getUserSettings()->setValue (deviceStateKey, deviceState.get());
        appProperties.saveIfNeeded();
    }

    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    smoothedMasterGain.reset (sampleRate, 0.05); // 50 ms ramps, so slider moves don't click
    smoothedMasterGain.setCurrentAndTargetValue (masterGain);

    engine.prepare (sampleRate, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto& buffer = *bufferToFill.buffer;
    const auto start = bufferToFill.startSample;
    const auto numSamples = bufferToFill.numSamples;

    if (buffer.getNumChannels() == 0)
        return;

    storeMax (inputPeak, buffer.getMagnitude (0, start, numSamples));

    engine.process (buffer, start, numSamples);

    // Master gain on the engine's mono result (channel 0), ramped to avoid clicks
    smoothedMasterGain.setTargetValue (masterGain);
    const auto gainStart = smoothedMasterGain.getCurrentValue();
    smoothedMasterGain.skip (numSamples);
    buffer.applyGainRamp (0, start, numSamples, gainStart, smoothedMasterGain.getCurrentValue());

    storeMax (outputPeak, buffer.getMagnitude (0, start, numSamples));

    // Mono -> every output channel
    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
        buffer.copyFrom (channel, start, buffer, 0, start, numSamples);
}

void MainComponent::releaseResources()
{
    engine.release();
}

//==============================================================================
void MainComponent::timerCallback()
{
    inputMeter.setLevel (inputPeak.exchange (0.0f));
    outputMeter.setLevel (outputPeak.exchange (0.0f));

    const float frequency = engine.getParameters().detectedFrequency;
    pitchLabel.setText (describePitch (frequency), juce::dontSendNotification);
    updateVoicesLabel (frequency);
}

void MainComponent::updateVoicesLabel (float inputFrequency)
{
    const auto& notes = engine.getNoteController();
    const int numVoices = notes.getNumActiveVoices();
    const bool transposed = NoteController::getConcertOffset (notes.getTransposition()) != 0;

    juce::String text;
    text << "Harmony voices " << numVoices << "/" << NoteController::maxVoices << ":  ";

    if (numVoices == 0)
        text << "none (click keys to add)";

    for (int voice = 0; voice < numVoices; ++voice)
    {
        text << (voice > 0 ? ",   " : "")
             << juce::MidiMessage::getMidiNoteName (notes.getWrittenNoteFor (voice), false, true, 4);

        if (transposed)
            text << " (concert " << juce::MidiMessage::getMidiNoteName (notes.getConcertNoteFor (voice), false, true, 4) << ")";

        // With a detected input note, also show how far the input gets shifted
        if (inputFrequency > 0.0f)
        {
            const auto shift = notes.getSemitoneShiftFor (voice, inputFrequency);
            text << " " << (shift >= 0.0f ? "+" : "") << juce::String (shift, 1) << " st";
        }
    }

    voicesLabel.setText (text, juce::dontSendNotification);
}

void MainComponent::setUpGainSlider (juce::Slider& slider, juce::Label& label, const juce::String& name,
                                     double initialDb, std::function<void (float)> onGainChange)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    slider.setRange (minGainDb, 6.0, 0.1);
    slider.textFromValueFunction = [] (double db)
    {
        return db <= minGainDb ? juce::String ("-inf dB") : juce::String (db, 1) + " dB";
    };
    slider.onValueChange = [&slider, onGainChange]
    {
        onGainChange (juce::Decibels::decibelsToGain ((float) slider.getValue(), minGainDb));
    };
    slider.setDoubleClickReturnValue (true, initialDb);
    slider.setValue (initialDb, juce::sendNotificationSync);
    slider.updateText(); // apply textFromValueFunction to the current value
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.attachToComponent (&slider, true);
}

void MainComponent::showAudioSettings()
{
    if (settingsWindow != nullptr)
    {
        settingsWindow->toFront (true);
        return;
    }

    // Exactly one input channel (the mic) and a stereo output pair
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (deviceManager,
                                                                          numInputChannels, numInputChannels,
                                                                          numOutputChannels, numOutputChannels,
                                                                          false, false, true, false);
    selector->setSize (520, 420);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector.release());
    options.dialogTitle = "Audio Settings";
    options.dialogBackgroundColour = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    settingsWindow = options.launchAsync();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto top = area.removeFromTop (44);
    settingsButton.setBounds (top.removeFromRight (150).withSizeKeepingCentre (150, 30));
    titleLabel.setBounds (top.removeFromLeft (220));
    pitchLabel.setBounds (top);

    area.removeFromTop (8);
    auto transpositionRow = area.removeFromTop (28);
    transpositionBox.setBounds (transpositionRow.removeFromRight (150));
    transpositionLabel.setBounds (transpositionRow.removeFromRight (100));

    area.removeFromTop (8);
    keyboard.setBounds (area.removeFromTop (200));

    area.removeFromTop (8);
    auto voicesRow = area.removeFromTop (30);
    clearButton.setBounds (voicesRow.removeFromRight (80));
    voicesLabel.setBounds (voicesRow);

    area.removeFromTop (16);
    auto meters = area.removeFromRight (area.getWidth() / 2 - 20);
    auto sliders = area;

    constexpr int labelWidth = 70;
    for (auto* slider : { &dryGainSlider, &harmonyGainSlider, &masterGainSlider })
    {
        slider->setBounds (sliders.removeFromTop (32).withTrimmedLeft (labelWidth));
        sliders.removeFromTop (8);
    }

    for (auto [meter, label] : { std::pair { &inputMeter, &inputMeterLabel },
                                 std::pair { &outputMeter, &outputMeterLabel } })
    {
        auto row = meters.removeFromTop (32);
        label->setBounds (row.removeFromLeft (labelWidth));
        meter->setBounds (row.reduced (0, 8));
        meters.removeFromTop (8);
    }
}
