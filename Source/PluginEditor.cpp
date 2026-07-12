#include "PluginEditor.h"

SoundShifterProAudioProcessorEditor::SoundShifterProAudioProcessorEditor(
    SoundShifterProAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(760, 430, 1180, 680);
    setSize(920, 520);

    brandLabel.setText("HAI PHAM AUDIO", juce::dontSendNotification);
    brandLabel.setFont(SoundShifterTheme::labelFont(11.0f));
    brandLabel.setColour(juce::Label::textColourId, SoundShifterTheme::accent);
    brandLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(brandLabel);

    productLabel.setText("SOUNDSHIFTER PRO", juce::dontSendNotification);
    productLabel.setFont(SoundShifterTheme::titleFont(25.0f));
    productLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(productLabel);

    configureKnob(pitchSlider, pitchCaption, "PITCH SHIFT", " st", true);
    configureKnob(fineSlider, fineCaption, "FINE", " ct", false);
    configureKnob(mixSlider, mixCaption, "MIX", " %", false);
    configureKnob(outputSlider, outputCaption, "OUTPUT", " dB", false);

    hqButton.setClickingTogglesState(true);
    bypassButton.setClickingTogglesState(true);
    addAndMakeVisible(hqButton);
    addAndMakeVisible(bypassButton);

    inputCaption.setText("INPUT", juce::dontSendNotification);
    outputMeterCaption.setText("OUTPUT", juce::dontSendNotification);
    for (auto* label : { &inputCaption, &outputMeterCaption })
    {
        label->setFont(SoundShifterTheme::labelFont(10.0f));
        label->setColour(juce::Label::textColourId, SoundShifterTheme::textMuted);
        label->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*label);
    }

    latencyLabel.setFont(SoundShifterTheme::labelFont(10.0f));
    latencyLabel.setColour(juce::Label::textColourId, SoundShifterTheme::textMuted);
    latencyLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(latencyLabel);

    engineLabel.setText("PITCH ENGINE V1 + MIDI", juce::dontSendNotification);
    engineLabel.setFont(SoundShifterTheme::labelFont(10.0f));
    engineLabel.setColour(juce::Label::textColourId, SoundShifterTheme::accent);
    engineLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(engineLabel);

    versionLabel.setText("v0.2.3", juce::dontSendNotification);
    versionLabel.setFont(SoundShifterTheme::labelFont(10.0f));
    versionLabel.setColour(juce::Label::textColourId, SoundShifterTheme::textMuted);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(versionLabel);

    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    auto& state = processor.getAPVTS();
    pitchAttachment = std::make_unique<SliderAttachment>(state, "pitch", pitchSlider);
    fineAttachment = std::make_unique<SliderAttachment>(state, "fine", fineSlider);
    mixAttachment = std::make_unique<SliderAttachment>(state, "mix", mixSlider);
    outputAttachment = std::make_unique<SliderAttachment>(state, "output", outputSlider);
    hqAttachment = std::make_unique<ButtonAttachment>(state, "hq", hqButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(state, "bypass", bypassButton);

    startTimerHz(30);
}

SoundShifterProAudioProcessorEditor::~SoundShifterProAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SoundShifterProAudioProcessorEditor::configureKnob(juce::Slider& slider,
                                                         juce::Label& label,
                                                         const juce::String& labelText,
                                                         const juce::String& suffix,
                                                         bool isPrimary)
{
    slider.setName(isPrimary ? "PITCH" : labelText);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f,
                               true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                           isPrimary ? 112 : 86,
                           isPrimary ? 30 : 25);
    slider.setNumDecimalPlacesToDisplay(labelText == "PITCH SHIFT" ? 0 : 1);
    slider.setTextValueSuffix(suffix);
    slider.setDoubleClickReturnValue(true, labelText == "MIX" ? 100.0 : 0.0);
    slider.setMouseDragSensitivity(isPrimary ? 220 : 180);
    addAndMakeVisible(slider);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(SoundShifterTheme::labelFont(isPrimary ? 12.0f : 11.0f));
    label.setColour(juce::Label::textColourId,
                    isPrimary ? SoundShifterTheme::text : SoundShifterTheme::textMuted);
    addAndMakeVisible(label);
}

void SoundShifterProAudioProcessorEditor::drawPanel(juce::Graphics& g,
                                                     juce::Rectangle<float> area,
                                                     float radius) const
{
    juce::DropShadow shadow(juce::Colours::black.withAlpha(0.40f), 14, { 0, 5 });
    shadow.drawForRectangle(g, area.toNearestInt());
    g.setColour(SoundShifterTheme::panel);
    g.fillRoundedRectangle(area, radius);
    g.setColour(SoundShifterTheme::outline.withAlpha(0.75f));
    g.drawRoundedRectangle(area, radius, 1.0f);
}

void SoundShifterProAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(SoundShifterTheme::background);

    auto full = getLocalBounds().toFloat();
    juce::ColourGradient backgroundGlow(SoundShifterTheme::panelRaised.withAlpha(0.55f),
                                        full.getCentreX(), full.getY(),
                                        SoundShifterTheme::background,
                                        full.getCentreX(), full.getBottom(), false);
    g.setGradientFill(backgroundGlow);
    g.fillRect(full);

    auto content = full.reduced(18.0f);
    auto header = content.removeFromTop(68.0f);
    auto footer = content.removeFromBottom(40.0f);
    content.removeFromTop(10.0f);
    content.removeFromBottom(10.0f);

    drawPanel(g, content, 14.0f);

    g.setColour(SoundShifterTheme::outline.withAlpha(0.6f));
    g.drawHorizontalLine(static_cast<int>(header.getBottom()),
                         18.0f, getWidth() - 18.0f);
    g.drawHorizontalLine(static_cast<int>(footer.getY()),
                         18.0f, getWidth() - 18.0f);

    g.setColour(SoundShifterTheme::textMuted.withAlpha(0.22f));
    g.setFont(10.0f);
    const auto centreX = static_cast<int>(getWidth() * 0.5f);
    g.drawText("POLYPHONIC PITCH ENGINE", centreX - 110, 78, 220, 18,
               juce::Justification::centred);

    g.setColour(SoundShifterTheme::textMuted);
    g.setFont(9.0f);
    g.drawText("Copyright Hai Pham", 0, getHeight() - 22, getWidth() - 20, 16,
               juce::Justification::centredRight);
}

void SoundShifterProAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);
    auto header = area.removeFromTop(58);

    brandLabel.setBounds(header.removeFromLeft(122));
    productLabel.setBounds(header.removeFromLeft(320));
    bypassButton.setBounds(header.removeFromRight(88).reduced(2, 12));
    header.removeFromRight(8);
    hqButton.setBounds(header.removeFromRight(54).reduced(2, 12));

    auto footer = area.removeFromBottom(36);
    latencyLabel.setBounds(footer.removeFromLeft(210));
    versionLabel.setBounds(footer.removeFromRight(90));
    engineLabel.setBounds(footer.withSizeKeepingCentre(150, footer.getHeight()));

    area.removeFromTop(18);
    area.removeFromBottom(14);
    area.reduce(18, 18);

    const auto w = area.getWidth();
    const auto h = area.getHeight();
    const auto centreWidth = juce::jlimit(220, 330, static_cast<int>(w * 0.34f));
    auto left = area.removeFromLeft((w - centreWidth) / 2);
    auto centre = area.removeFromLeft(centreWidth);
    auto right = area;

    centre.reduce(10, 4);
    pitchCaption.setBounds(centre.removeFromTop(24));
    pitchSlider.setBounds(centre.reduced(6, 0));

    auto leftTop = left.removeFromTop(static_cast<int>(h * 0.58f));
    auto leftBottom = left;
    leftTop.reduce(10, 0);
    fineCaption.setBounds(leftTop.removeFromTop(22));
    fineSlider.setBounds(leftTop.reduced(10, 0));
    leftBottom.reduce(4, 8);
    inputCaption.setBounds(leftBottom.removeFromTop(18));
    inputMeter.setBounds(leftBottom.reduced(0, 4));

    auto rightTop = right.removeFromTop(static_cast<int>(h * 0.58f));
    auto mixArea = rightTop.removeFromLeft(rightTop.getWidth() / 2);
    auto outputArea = rightTop;
    mixArea.reduce(2, 0);
    outputArea.reduce(2, 0);
    mixCaption.setBounds(mixArea.removeFromTop(22));
    mixSlider.setBounds(mixArea.reduced(4, 0));
    outputCaption.setBounds(outputArea.removeFromTop(22));
    outputSlider.setBounds(outputArea.reduced(4, 0));

    auto rightBottom = right;
    rightBottom.reduce(4, 8);
    outputMeterCaption.setBounds(rightBottom.removeFromTop(18));
    outputMeter.setBounds(rightBottom.reduced(0, 4));
}

void SoundShifterProAudioProcessorEditor::timerCallback()
{
    const auto smooth = [](float target, float current)
    {
        return juce::jmax(target, current - 1.5f);
    };

    displayedInputLeftDb = smooth(processor.getInputLeftDb(), displayedInputLeftDb);
    displayedInputRightDb = smooth(processor.getInputRightDb(), displayedInputRightDb);
    displayedOutputLeftDb = smooth(processor.getOutputLeftDb(), displayedOutputLeftDb);
    displayedOutputRightDb = smooth(processor.getOutputRightDb(), displayedOutputRightDb);

    inputMeter.setLevels(displayedInputLeftDb, displayedInputRightDb);
    outputMeter.setLevels(displayedOutputLeftDb, displayedOutputRightDb);

    const auto sampleRate = juce::jmax(1.0, processor.getCurrentSampleRateHz());
    const auto latencyMs = 1000.0 * static_cast<double>(processor.getLatencySamples()) / sampleRate;
    latencyLabel.setText("LATENCY  " + juce::String(latencyMs, 1) + " ms",
                         juce::dontSendNotification);
}
