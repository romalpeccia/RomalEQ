/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
enum Channel {
    Right, // represented as 0
    Left // represented as 1
};


// class for GUI to retrieve blocks produced by single channel FIFO class
template<typename T>
struct Fifo
{
    void prepare(int numChannels, int numSamples)
    {
        static_assert(std::is_same_v<T, juce::AudioBuffer<float>>,
            "prepare(numChannels, numSamples) should only be used when the Fifo is holding juce::AudioBuffer<float>");
        for (auto& buffer : buffers)
        {
            buffer.setSize(numChannels,
                numSamples,
                false,   //clear everything?
                true,    //including the extra space?
                true);   //avoid reallocating if you can?
            buffer.clear();
        }
    }

    void prepare(size_t numElements)
    {
        static_assert(std::is_same_v<T, std::vector<float>>,
            "prepare(numElements) should only be used when the Fifo is holding std::vector<float>");
        for (auto& buffer : buffers)
        {
            buffer.clear();
            buffer.resize(numElements, 0);
        }
    }

    bool push(const T& t)
    {
        auto write = fifo.write(1);
        if (write.blockSize1 > 0)
        {
            buffers[write.startIndex1] = t;
            return true;
        }

        return false;
    }

    bool pull(T& t)
    {
        auto read = fifo.read(1);
        if (read.blockSize1 > 0)
        {
            t = buffers[read.startIndex1];
            return true;
        }

        return false;
    }

    int getNumAvailableForReading() const
    {
        return fifo.getNumReady();
    }
private:
    static constexpr int Capacity = 30;
    std::array<T, Capacity> buffers;
    juce::AbstractFifo fifo{ Capacity };
};


//FFT uses fixed number of samples, host is sending mixed size audio samples
//single channel sample fifo does this
template<typename BlockType>
struct SingleChannelSampleFifo
{
    SingleChannelSampleFifo(Channel ch) : channelToUse(ch)
    {
        prepared.set(false);
    }

    void update(const BlockType& buffer)
    {
        jassert(prepared.get());
        jassert(buffer.getNumChannels() > channelToUse);
        auto* channelPtr = buffer.getReadPointer(channelToUse);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            pushNextSampleIntoFifo(channelPtr[i]);
        }
    }

    void prepare(int bufferSize)
    {
        prepared.set(false);
        size.set(bufferSize);

        bufferToFill.setSize(1,             //channel
            bufferSize,    //num samples
            false,         //keepExistingContent
            true,          //clear extra space
            true);         //avoid reallocating
        audioBufferFifo.prepare(1, bufferSize);
        fifoIndex = 0;
        prepared.set(true);
    }
    //==============================================================================
    int getNumCompleteBuffersAvailable() const { return audioBufferFifo.getNumAvailableForReading(); }
    bool isPrepared() const { return prepared.get(); }
    int getSize() const { return size.get(); }
    //==============================================================================
    bool getAudioBuffer(BlockType& buf) { return audioBufferFifo.pull(buf); }
private:
    Channel channelToUse;
    int fifoIndex = 0;
    Fifo<BlockType> audioBufferFifo;
    BlockType bufferToFill;
    juce::Atomic<bool> prepared = false;
    juce::Atomic<int> size = 0;

    void pushNextSampleIntoFifo(float sample)
    {
        if (fifoIndex == bufferToFill.getNumSamples())
        {
            auto ok = audioBufferFifo.push(bufferToFill);

            juce::ignoreUnused(ok);

            fifoIndex = 0;
        }

        bufferToFill.setSample(0, fifoIndex, sample);
        ++fifoIndex;
    }
};


enum Slope {
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};

//data structure representing apvts parameter values
struct ChainSettings
{
    float peakFreq{ 0 }, peakGainInDecibels{ 0 }, peakQuality{ 1.f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    Slope lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };

};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts );

//create type aliases to simplify definitions
using Filter = juce::dsp::IIR::Filter<float>;

//important JUCE dsp concept, define a processing chain and then pass in a processing context
//4 filters in a CutFilter because TODO ??????????
using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
//mono chain: lowcut -> parametric band -> highcut
using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
//two monochains needed for stereo 

//define chain Positions
enum ChainPositions {
    LowCut,
    Peak,
    HighCut
};



using Coefficients = Filter::CoefficientsPtr;
void updateCoefficients(Coefficients& old, const Coefficients& replacements);

Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate);



//template arguments help compiler deduce arguments? TODO understand templates
template<int Index, typename ChainType, typename CoefficientType>
void update(ChainType& chain, const CoefficientType& Coefficients) {
    updateCoefficients(chain.template get<Index>().coefficients, Coefficients[Index]);
    chain.template setBypassed < Index>(false);
}

template<typename ChainType, typename CoefficientType>
void updateCutFilter(ChainType& chain, const CoefficientType& coefficients, const Slope& slope)
{
    chain.template setBypassed<0>(true);
    chain.template setBypassed<1>(true);
    chain.template setBypassed<2>(true);
    chain.template setBypassed<3>(true);
    switch (slope)
    {
        case Slope_48: {

            //*leftLowCut.template get<3>().coefficients = *coefficients[3];
            //leftLowCut.template setBypassed<3>(false);
            update<3>(chain, coefficients);
        }
        case Slope_36: {
            update<2>(chain, coefficients);
        }
        case Slope_24: {
            update<1>(chain, coefficients);
        }
        case Slope_12: {
            update<0>(chain, coefficients);
        }
    }
}

inline auto makeLowCutFilter(const ChainSettings& chainSettings, double sampleRate) {
    return  juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
       sampleRate, (chainSettings.lowCutSlope + 1) * 2);
}

inline auto makeHighCutFilter(const ChainSettings& chainSettings, double sampleRate) {
    return  juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
        sampleRate, (chainSettings.highCutSlope + 1) * 2);
}






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

    using BlockType = juce::AudioBuffer<float>;
    SingleChannelSampleFifo<BlockType> leftChannelFifo { Channel::Left };
    SingleChannelSampleFifo<BlockType> rightChannelFifo {  Channel::Right };


private:

        //enums and type aliases moved outside class

        MonoChain leftChain, rightChain;

        void updatePeakFilter(const ChainSettings& chainSettings);


        void updateLowCutFilters(const ChainSettings& chainSettings);
        void updateHighCutFilters(const ChainSettings& chainSettings);

        void updateFilters();

        //produce a sin wave on our grid to debug FFT visualizer?
        juce::dsp::Oscillator<float> osc;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RomalEQAudioProcessor)
};


/*
NOTES:





attach params to sliders
create chain for editor (for visualization)
draw visualizer in paint()
attach slider params to chain in editor (not just chain in processor)

*/