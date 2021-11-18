/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//data structure representing apvts parameter values

enum Slope {
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};
struct ChainSettings
{
    float peakFreq{ 0 }, peakGainInDecibels{ 0 }, peakQuality{ 1.f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    Slope lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };

};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts );





//==============================================================================
/**
*/
class RomalEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    RomalEQAudioProcessor();
    ~RomalEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;


    //NON DEFAULT CODE BELOW
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{ *this, nullptr, "Parameters", createParameterLayout() };

private:

    //create type aliases to simplify definitions
        using Filter = juce::dsp::IIR::Filter<float>;

        //important JUCE dsp concept, define a processing chain and then pass in a processing context
        //4 filters in a CutFilter because TODO ??????????
        using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
        //mono chain: lowcut -> parametric band -> highcut
        using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
        //two monochains needed for stereo 
        MonoChain leftChain, rightChain;


        //define chain Positions
        enum ChainPositions {
            LowCut,
            Peak,
            HighCut
        };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomalEQAudioProcessor)
};








/*
PERSONAL NOTES







*/