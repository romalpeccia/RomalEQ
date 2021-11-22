/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*
*/


//custom slider object that we can reuse, that inherits juce Slider
struct CustomRotarySlider : juce::Slider
{
    CustomRotarySlider() : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag, juce::Slider::TextEntryBoxPosition::NoTextBox) {

    }
};


class RomalEQAudioProcessorEditor  : public juce::AudioProcessorEditor,
    juce::AudioProcessorParameter::Listener,
    juce::Timer
{
public:
    RomalEQAudioProcessorEditor (RomalEQAudioProcessor&);
    ~RomalEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;


    //taken from juce::AudioProcessorParameter::Listener class
        void parameterValueChanged(int parameterIndex, float newValue) override;
        //empty implementation because we dont care about this function TODO ? virtual keyword means we have to write it because ??
            void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {} 
    void timerCallback() override;



private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    RomalEQAudioProcessor& audioProcessor;
    juce::Atomic<bool> parametersChanged{ false };

    CustomRotarySlider peakFreqSlider, peakGainSlider, peakQualitySlider, lowCutFreqSlider, highCutFreqSlider, lowCutSlopeSlider, highCutSlopeSlider;
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment peakFreqSliderAttachment, peakGainSliderAttachment, peakQualitySliderAttachment,
        lowCutFreqSliderAttachment, highCutFreqSliderAttachment, lowCutSlopeSliderAttachment, highCutSlopeSliderAttachment;


    //put them in a vector to iterate through them easily
    std::vector<juce::Component*> getComps();

    //editor has its own monochain
    MonoChain monoChain;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomalEQAudioProcessorEditor)
};
