#include "PluginEditor.h"

SoundShifterProAudioProcessorEditor::SoundShifterProAudioProcessorEditor(
    SoundShifterProAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    setOpaque(true);
    setResizable(false, false);
    setSize(720, 410);

    titleLabel.setText("SOUND SHIFTER PRO", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("BEAT PITCH ENGINE", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7f8994));
    subtitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel);

    configureKnob(pitchSlider, pitchLabel, "PITCH", " st");
    configureKnob(fineSlider, fineLabel, "FINE", " ct");
    configureKnob(mixSlider, mixLabel, "MIX", " %");
    configureKnob(outputSlider, outputLabel, "OUTPUT", " dB");

    bypassButton.setClickingTogglesState(true);
    addAndMakeVisible(bypassButton);

    auto& state = processor.getAPVTS();
    pitchAttachment = std::make_unique<SliderAttachment>(state, "pitch", pitchSlider);
    fineAttachment = std::make_unique<SliderAttachment>(state, "fine", fineSlider);
    mixAttachment = std::make_unique<SliderAttachment>(state, "mix", mixSlider);
    outputAttachment = std::make_unique<SliderAttachment>(state, "output", outputSlider);
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
                                                         const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 24);
    slider.setNumDecimalPlacesToDisplay(labelText == "PITCH" ? 0 : 1);
    slider.setTextValueSuffix(suffix);
    slider.setDoubleClickReturnValue(true, labelText == "MIX" ? 100.0 : 0.0);
    addAndMakeVisible(slider);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    addAndMakeVisible(label);
}

void SoundShifterProAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101419));

    auto panel = getLocalBounds().toFloat().reduced(18.0f);
    g.setColour(juce::Colour(0xff171c22));
    g.fillRoundedRectangle(panel, 12.0f);

    g.setColour(juce::Colour(0xff2b323a));
    g.drawRoundedRectangle(panel, 12.0f, 1.0f);

    auto meterArea = juce::Rectangle<float>(34.0f, 330.0f, 652.0f, 42.0f);
    drawMeter(g, meterArea.removeFromLeft(310.0f), displayedInputDb, "INPUT");
    meterArea.removeFromLeft(32.0f);
    drawMeter(g, meterArea, displayedOutputDb, "OUTPUT");

    g.setColour(juce::Colour(0xff69737e));
    g.setFont(10.0f);
    g.drawText("Milestone 1 • DSP BYPASS", 26, 383, 220, 16, juce::Justification::centredLeft);
    g.drawText("Copyright Hai Pham", 474, 383, 220, 16, juce::Justification::centredRight);
}

void SoundShifterProAudioProcessorEditor::drawMeter(juce::Graphics& g,
                                                     juce::Rectangle<float> area,
                                                     float levelDb,
                                                     const juce::String& title) const
{
    g.setColour(juce::Colour(0xff818b96));
    g.setFont(10.0f);
    g.drawText(title, area.removeFromLeft(48.0f), juce::Justification::centredLeft);

    const auto background = area.reduced(0.0f, 13.0f);
    g.setColour(juce::Colour(0xff090c0f));
    g.fillRoundedRectangle(background, 3.0f);

    const auto normalised = juce::jlimit(0.0f, 1.0f, juce::jmap(levelDb, -60.0f, 0.0f, 0.0f, 1.0f));
    auto fill = background;
    fill.setWidth(background.getWidth() * normalised);
    g.setColour(juce::Colour(0xff5cb8ff));
    g.fillRoundedRectangle(fill, 3.0f);
}

void SoundShifterProAudioProcessorEditor::resized()
{
    titleLabel.setBounds(180, 28, 360, 32);
    subtitleLabel.setBounds(260, 58, 200, 18);

    constexpr int knobY = 112;
    constexpr int knobWidth = 132;
    constexpr int knobHeight = 154;
    constexpr int labelY = 92;

    pitchLabel.setBounds(68, labelY, knobWidth, 20);
    pitchSlider.setBounds(68, knobY, knobWidth, knobHeight);

    fineLabel.setBounds(220, labelY, knobWidth, 20);
    fineSlider.setBounds(220, knobY, knobWidth, knobHeight);

    mixLabel.setBounds(372, labelY, knobWidth, 20);
    mixSlider.setBounds(372, knobY, knobWidth, knobHeight);

    outputLabel.setBounds(524, labelY, knobWidth, 20);
    outputSlider.setBounds(524, knobY, knobWidth, knobHeight);

    bypassButton.setBounds(290, 282, 140, 32);
}

void SoundShifterProAudioProcessorEditor::timerCallback()
{
    const auto inputTarget = processor.getInputLevelDb();
    const auto outputTarget = processor.getOutputLevelDb();

    displayedInputDb = juce::jmax(inputTarget, displayedInputDb - 1.8f);
    displayedOutputDb = juce::jmax(outputTarget, displayedOutputDb - 1.8f);
    repaint();
}
