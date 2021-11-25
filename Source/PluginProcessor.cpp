/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
RomalEQAudioProcessor::RomalEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

RomalEQAudioProcessor::~RomalEQAudioProcessor()
{
}

//==============================================================================
const juce::String RomalEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RomalEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool RomalEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool RomalEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double RomalEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RomalEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int RomalEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RomalEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String RomalEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void RomalEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void RomalEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    //prepare process spec object and pass it to chain, which then passes into each link in the chain
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    updateFilters();


    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);
}

void RomalEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool RomalEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void RomalEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());



    //update parameters before running audio through them
    updateFilters();

    //processor chain requires processing context to be passed into it in order to run audio through links in the chain
    // processing context needs an audio block instance
    // audio block is initialized with whatever processBlock buffer we are given by the audioprocessor
    //audio block -> seperate into channel blocks -> pass into contexts -> initialize mono chains with context
    juce::dsp::AudioBlock<float> block(buffer);
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    leftChain.process(leftContext);
    rightChain.process(rightContext);

    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);

}

//==============================================================================
bool RomalEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* RomalEQAudioProcessor::createEditor()
{
    //uncomment for generic GUI that shows variables
    //return new juce::GenericAudioProcessorEditor(*this);
    
    return new RomalEQAudioProcessorEditor (*this);
   

}

//==============================================================================
void RomalEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    //store apvts to memory output stream
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);

}

void RomalEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    //check if tree is valid before copying to plugin
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        apvts.replaceState(tree);
        updateFilters();
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RomalEQAudioProcessor();
}



/*

HELPER FUNCTIONS














*/
juce::AudioProcessorValueTreeState::ParameterLayout RomalEQAudioProcessor::createParameterLayout() {
    //initializes parameter layout

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    //std::make_unique<juce::AudioParameterFloat>
        //param name, display name, range, default value
        //normalisableRange(low, high, stepsize, skew)
        //skew factor handles the rate at which the slider changes over the range given
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq", "HighCut Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20000.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq", "Peak Freq", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 750.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain", "Peak Gain", juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality", "Peak Quality", juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.0f), 1.0f));
    
    juce::StringArray stringArray;
    for (int i = 0; i < 4; ++i) {
        juce::String str;
        str << (12 + i * 12);
        str << " db/Oct";
        stringArray.add(str);
    }
    // std::make_unique<juce::AudioParameterChoice>
        // param name, display name, choice array of strings, default value
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));
    return layout;

}


ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    //puts apvts params into our Chain Settings struct for cleaner code

    ChainSettings settings;
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());

    return settings;
}


Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate) {

    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, chainSettings.peakFreq, chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
}



void RomalEQAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings) {

    //auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), chainSettings.peakFreq, chainSettings.peakQuality,
    //    juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));

    auto peakCoefficients = makePeakFilter(chainSettings, getSampleRate());

    //*leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    //*rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;

    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void /*RomalEQAudioProcessor::*/updateCoefficients(Coefficients& old, const Coefficients& replacements) {
    *old = *replacements;
}


void RomalEQAudioProcessor::updateLowCutFilters(const ChainSettings& chainSettings) {

        //butterworthmethod args
    //frequency, samplerate, order
    // order returns order/2 sets of coefficients
    //slope is 0,1,2,3 (representing 12, 24, 36, 48)
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, getSampleRate());
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    updateCutFilter(leftLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(rightLowCut, lowCutCoefficients, chainSettings.lowCutSlope);




}
void RomalEQAudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings) {

    //butterworthmethod args
    //frequency, samplerate, order
    // order returns order/2 sets of coefficients
    //slope is 0,1,2,3 (representing 12, 24, 36, 48)
    auto highCutCoefficients = makeHighCutFilter(chainSettings, getSampleRate());
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);

}

void RomalEQAudioProcessor::updateFilters() {
    auto chainSettings = getChainSettings(apvts);

    updateLowCutFilters(chainSettings);
    updateHighCutFilters(chainSettings);
    updatePeakFilter(chainSettings);

}