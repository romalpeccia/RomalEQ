/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
RomalEQAudioProcessorEditor::RomalEQAudioProcessorEditor (RomalEQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    //why is there code outside of the function block?
    

    //attach apvts params to sliders
    peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
    peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
    peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
    lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
    highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
    lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
    highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.



     
    //iterate through the vector of sliders we made 
    for (auto* comp : getComps()) 
    {
        addAndMakeVisible(comp);
    }



    //listen for parameter changes
    const auto& params = audioProcessor.getParameters();
    for (auto param : params) {
        param->addListener(this);
    }
    startTimerHz(60);

    setSize(600, 400);
}

RomalEQAudioProcessorEditor::~RomalEQAudioProcessorEditor()
{
    const auto& params = audioProcessor.getParameters();
    for (auto param : params) {
        param->removeListener(this);
    }
}

//==============================================================================
void RomalEQAudioProcessorEditor::paint (juce::Graphics& g) 
{

    using namespace juce;
    g.fillAll (Colours::black);

    //making visualizer
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    auto w = responseArea.getWidth();
    auto& lowcut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highcut = monoChain.get<ChainPositions::HighCut>();
    auto sampleRate = audioProcessor.getSampleRate();
    std::vector<double> mags;
    mags.resize(w);

    //mapping pixels 
    for (int i = 0; i < w; ++i) {
        double mag = 1.f;
        auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);
        if (! monoChain.isBypassed<ChainPositions::Peak>()) 
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        if (! lowcut.isBypassed<0>() )
             mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowcut.isBypassed<1>())
            mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowcut.isBypassed<2>())
            mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowcut.isBypassed<3>())
            mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (!highcut.isBypassed<0>())
            mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highcut.isBypassed<1>())
            mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highcut.isBypassed<2>())
            mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highcut.isBypassed<3>())
            mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        mags[i] = Decibels::gainToDecibels(mag);


        Path responseCurve;
        const double outputMin = responseArea.getBottom();
        const double outputMax = responseArea.getY();
        //map decibels to screen coords
        auto map = [outputMin, outputMax](double input) {
            return jmap(input, -20.0, 24.0, outputMin, outputMax);
        };
        responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));
        for (size_t i = 1; i < mags.size(); ++i) {
            responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
        }

        g.setColour(Colours::orange);
        g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);
        g.setColour(Colours::white);
        g.strokePath(responseCurve, PathStrokeType(2.f));
    }

    //end making visualizer
}

void RomalEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    //reserve top 1/3 for visualizer
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);

    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight()*0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);
    //every time we remove from bounds, the area of bounds changes 
    //first we remove 1/3 * 1 unit area, then we remove 0.5 * 2/3rd unit area
    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQualitySlider.setBounds(bounds);

}

void RomalEQAudioProcessorEditor::parameterValueChanged(int parameterIndex, float newValue) {

    parametersChanged.set(true);
}
void RomalEQAudioProcessorEditor::timerCallback() {
    if (parametersChanged.compareAndSetBool(false, true));
    {
        DBG("params changed");
        //update the monochain in the editor
            // get chain settings and coefficients from audioProcessor and use them to update editor chain
        auto chainSettings = getChainSettings(audioProcessor.apvts);
        auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
            // update monochain
        updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
        //signal a repaint
        repaint();
    }


}

std::vector<juce::Component*> RomalEQAudioProcessorEditor::getComps() {
    return{
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutSlopeSlider

    };

}