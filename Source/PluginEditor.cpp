/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
//==============================================================================
//Custom Sliders
void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPosProportional, float rotaryStartAngle,
    float rotaryEndAngle, juce::Slider& slider) 
{
    using namespace juce;
    auto bounds = Rectangle<float>(x, y, width, height);
    g.setColour(Colour(48u, 9u, 84));
    g.fillEllipse(bounds);

    g.setColour(Colour(128u, 77u, 1u));
    g.drawEllipse(bounds, 1.f);


    //???if we can cast rswl to rotaryslider with labels then we can use its functions??? why is this needed
    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider)) {
       
    //make main rectangle
        auto center = bounds.getCentre();

        Path p;
        Rectangle<float> r;
        r.setLeft(center.getX() - 2);
        r.setRight(center.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(center.getY()  - rswl->getTextHeight()*1.5); // subtract text height
        p.addRoundedRectangle(r, 2.f);
        jassert(rotaryStartAngle < rotaryEndAngle);

        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);
        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));
        g.fillPath(p);

    // make value text rectangle
        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);
        r.setSize(strWidth + 4, rswl->getTextHeight() + 2);
        r.setCentre(center);
        g.setColour(Colour(48u, 9u, 84));
        g.fillRect(r);
        g.setColour(Colours::orange);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

void CustomLookAndFeel::drawToggleButton(juce::Graphics& g,
    juce::ToggleButton& toggleButton,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    using namespace juce;
    Path powerButton;
    auto bounds = toggleButton.getLocalBounds();
    auto size = jmin(bounds.getWidth(), bounds.getHeight()) - 6;
    auto r = bounds.withSizeKeepingCentre(size, size).toFloat();
    float ang = 30.f;
    size -= 6;
    powerButton.addCentredArc(r.getCentreX(), r.getCentreY(), size * 0.5, size * 0.5, 0, degreesToRadians(ang), degreesToRadians(360.f - ang), true);
    powerButton.startNewSubPath(r.getCentreX(), r.getY());
    powerButton.lineTo(r.getCentre());
    PathStrokeType pst(2.f, PathStrokeType::JointStyle::curved);
    auto color = toggleButton.getToggleState() ? Colours::dimgrey : Colours::orange;
    g.setColour(color);
    g.strokePath(powerButton, pst);

    g.drawEllipse(r, 2);

}

//==============================================================================
void RotarySliderWithLabels::paint(juce::Graphics& g) {

    using namespace juce;
    auto startAngle = degreesToRadians(180.f + 45.f);
    auto endAngle = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi; // add two pi because the range needs to go from low to high
    auto range = getRange();
    auto sliderBounds = getSliderBounds();

    //bounding boxes to help debug/visualize
    /*
    g.setColour(Colours::red);
    g.drawRect(getLocalBounds());
    g.setColour(Colours::yellow);
    g.drawRect(sliderBounds);
    */
    getLookAndFeel().drawRotarySlider(g, sliderBounds.getX(), sliderBounds.getY(), sliderBounds.getWidth(), sliderBounds.getHeight(),
        jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0), 
        startAngle, endAngle, *this);
        
    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;

    g.setColour(Colours::white);
    g.setFont(getTextHeight());

    //iterate through labels and draw them?
    auto numChoices = labels.size();
    for (int i = 0; i < numChoices; ++i)
    {
        auto pos = labels[i].pos;
        jassert(0.f <= pos);
        jassert(pos <= 1.f);
        auto ang = jmap(pos, 0.f, 1.f, startAngle, endAngle);
        auto c = center.getPointOnCircumference(radius + getTextHeight() * 0.5f + 1, ang);

        Rectangle<float> r;
        auto str = labels[i].label;
        r.setSize(g.getCurrentFont().getStringWidth(str), getTextHeight());
        r.setCentre(c);
        r.setY(r.getY() + getTextHeight());
        g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);
    }

}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const {
    auto bounds = getLocalBounds();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    //make space for text?
    size -= getTextHeight() * 2;
    //make a square so our ellipse is a circle
    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2);

    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();
    juce::String str;
    bool addK = false;

    //failsafe check to see what type of param we are using
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float value = getValue();
        if (value > 999.f) {
            value /= 1000.f;
            addK = true;
        }
        str = juce::String(value, (addK ? 2 : 0)); //TODO: learn lambda? function
    }
    else
    {
        jassertfalse; // this shouldn't happen in this project unless I add new params
    }

    if (suffix.isNotEmpty()) {
        str << " ";
        if (addK)
            str << "k";
        str << suffix;
    }


    return str;
}


//END Custom Slider 

//==============================================================================
// Response Curve Code
//added editor component for response curve, copied over main editor code that controls the response curve

ResponseCurveComponent::ResponseCurveComponent(RomalEQAudioProcessor& p) : audioProcessor(p) 
//, leftChannelFifo(&audioProcessor.leftChannelFifo)
, leftPathProducer(audioProcessor.leftChannelFifo),
rightPathProducer(audioProcessor.rightChannelFifo)
{

    const auto& params = audioProcessor.getParameters();
    for (auto param : params) {
        param->addListener(this);
    }





    updateChain();
    startTimerHz(60);
}

ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    for (auto param : params) {
        param->removeListener(this);
    }
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue) {

    parametersChanged.set(true);
}

void PathProducer::process(juce::Rectangle<float> fftBounds, double sampleRate)
{
    //while there are buffers to pull, if we can pull buffer, send it to FFT data generator
    juce::AudioBuffer<float> tempIncomingBuffer;
    while (leftChannelFifo->getNumCompleteBuffersAvailable() > 0)
    {
        if (leftChannelFifo->getAudioBuffer(tempIncomingBuffer)) {
            //shift everything in monoBuffer to make space for incoming data
            auto size = tempIncomingBuffer.getNumSamples();
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0),
                monoBuffer.getReadPointer(0, size),
                monoBuffer.getNumSamples() - size);
            //copy incoming data
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size),
                tempIncomingBuffer.getReadPointer(0, 0),
                size);
            //pass it into FFT data generator
            leftChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f);
        }
    }
    //if there are FFT data buffers to pull
    // if we can pull a buffer, generate a path
    //const auto fftBounds = getAnalysisArea().toFloat();
    const auto fftSize = leftChannelFFTDataGenerator.getFFTSize();
    // 48000/2048 = 23 hz = bin width
    const auto binWidth = sampleRate / (double)fftSize;

    while (leftChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0)
    {
        std::vector<float> fftData;
        if (leftChannelFFTDataGenerator.getFFTData(fftData))
        {
            pathProducer.generatePath(fftData, fftBounds, fftSize, binWidth, -48.f);
        }

    }
    //path producer has produced us some number of path
    //we only want to display recent path
    while (pathProducer.getNumPathsAvailable())
    {
        pathProducer.getPath(leftChannelFFTPath);

    }
}


void ResponseCurveComponent::timerCallback() {
    

    auto fftBounds = getAnalysisArea().toFloat();
    auto sampleRate = audioProcessor.getSampleRate();
    leftPathProducer.process(fftBounds, sampleRate);
    rightPathProducer.process(fftBounds, sampleRate);


    if (parametersChanged.compareAndSetBool(false, true))
    {
        // DBG("params changed");
        updateChain();
        //signal a repaint
        //repaint();
    }
    repaint();




}

void ResponseCurveComponent::updateChain() {


 //update the monochain in the editor
     // get chain settings and coefficients from audioProcessor and use them to update editor chain
    auto chainSettings = getChainSettings(audioProcessor.apvts);


    monoChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    monoChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);

    auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
    // update monochain
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);


    auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());

    updateCutFilter(monoChain.get<ChainPositions::LowCut >(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(monoChain.get<ChainPositions::HighCut >(), highCutCoefficients, chainSettings.highCutSlope);
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{

    using namespace juce;
    g.fillAll(Colours::black);



    //draw Grid
    g.drawImage(background, getLocalBounds().toFloat());


    //making visualizer
    //auto responseArea = getLocalBounds();
     //auto responseArea = getRenderArea();
     auto responseArea = getAnalysisArea();
    //auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
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
        if (!monoChain.isBypassed<ChainPositions::Peak>())
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);



        if (!monoChain.isBypassed<ChainPositions::LowCut>())
        {
            if (!lowcut.isBypassed<0>())
                mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<1>())
                mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<2>())
                mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<3>())
                mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        if (!monoChain.isBypassed<ChainPositions::HighCut>())
        {
            if (!highcut.isBypassed<0>())
                mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<1>())
                mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<2>())
                mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<3>())
                mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        //TODO fix heights of curve so I don't have to have this +1 hack in here to get curve on 0 db line
        mags[i] = Decibels::gainToDecibels(mag) + 1;

    }
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

    //draw spectrum 
    //transform path to be at bottom and not at weird origin

    auto leftChannelFFTPath = leftPathProducer.getPath();
    leftChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX(), responseArea.getY()));

    g.setColour(Colours::skyblue);
    g.strokePath(leftChannelFFTPath, PathStrokeType(1.f));

    auto rightChannelFFTPath = rightPathProducer.getPath();
    rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX(), responseArea.getY()));

    g.setColour(Colours::lightyellow);
    g.strokePath(rightChannelFFTPath, PathStrokeType(1.f));

    //end draw spectrum


    g.setColour(Colours::orange);
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.f, 1.f);
    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.f));
//end making visualizer

}

void ResponseCurveComponent::resized()
{

    //making the grid
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    Graphics g(background);

    auto renderArea = getAnalysisArea();
    auto left = renderArea.getX();
    auto right = renderArea.getRight();
    auto top = renderArea.getY();
    auto bottom = renderArea.getBottom();
    auto width = renderArea.getWidth();






    //standardized frequency graphing scale
    Array<float> freqs{
        20, /*30, 40,*/ 50, 100, 200,/*300, 400,*/ 500, 1000, 2000, /*3000, 4000,*/ 5000, 10000, 20000
    };

    //store array of X values
    Array <float> xs;
    for (auto f : freqs)
    {
        auto normX = mapFromLog10(f, 20.f, 20000.f);
        xs.add(left + width * normX);
    }

    g.setColour(Colours::dimgrey);
    for (auto x : xs)
    {
        g.drawVerticalLine(x, top, bottom);
    }



    Array<float> gains{
        -24, -12, 0, 12, 24
    };
    for (auto gDB : gains)
    {
        auto y = jmap(gDB, -24.f, 24.f, float(bottom), float(top));
        //draw 0 dB differently for clairty
        g.setColour(gDB == 0.f ?  Colours::orange : Colours::darkgrey);
        g.drawHorizontalLine(y, left, right);

    }

    //draw debug border
    // g.drawRect(getAnalysisArea());

    //add labels to axes
    g.setColour(Colours::white);
    const int fontHeight = 10;
    g.setFont(fontHeight);
    //loop through frequencies and draw them
    for (int i = 0; i < freqs.size(); ++i)
    {
        auto f = freqs[i];
        auto x = xs[i];
        bool addK = false;
        String str;
        if (f > 999.f)
        {
            addK = true;
            f /= 1000.f;
        }
        str << f;
        if (addK)
            str << "k";
        str << "Hz";
        auto textWidth = g.getCurrentFont().getStringWidth(str);
        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setCentre(x, 0);  
        r.setY(1);
        g.drawFittedText(str, r, juce::Justification::centred, 1);
    }

    for (auto gDB : gains){
        auto y = jmap(gDB, -24.f, 24.f, float(bottom), float(top));
        String str;

        //draw leftside
        if (gDB > 0)
            str << "+";
        str << gDB;
        Rectangle <int> r;
        auto textWidth = g.getCurrentFont().getStringWidth(str);
        r.setSize(textWidth, fontHeight);
        r.setX(getWidth() - textWidth);
        r.setCentre(r.getCentreX(), y);

        g.setColour(gDB == 0.f ? Colours::orange : Colours::white);
        g.drawFittedText(str, r, juce::Justification::centred, 1);

        //draw right side (why is it different?)
        str.clear();
        str << (gDB - 24.f);
        r.setX(1);
        r.setCentre(r.getCentreX(), y);
        textWidth = g.getCurrentFont().getStringWidth(str);
        r.setSize(textWidth, fontHeight);
        g.setColour(Colours::grey);
        g.drawFittedText(str, r, juce::Justification::centred, 1);
    }
}


juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
    auto bounds = getLocalBounds();
    //bounds.reduce(JUCE_LIVE_CONSTANT(5),
    //    JUCE_LIVE_CONSTANT(5));
    //bounds.reduce(10,8 );

    bounds.removeFromTop(12);
    bounds.removeFromBottom(2);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);

    return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea() 
{
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
    return bounds;
}

//==============================================================================
// END Response Curve Code


//==============================================================================
RomalEQAudioProcessorEditor::RomalEQAudioProcessorEditor(RomalEQAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    //TODO understand why is there code outside of the function block?, part of constructor syntax?

    //create custom sliders
    peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz"),
    peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB"),
    peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), ""),
    lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
    highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
    lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "db/Oct"),
    highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "db/Oct"),



   
    responseCurveComponent(audioProcessor), 
    //attach apvts params to sliders
    peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
    peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
    peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
    lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
    highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
    lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
    highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider),
    lowcutButtonAttachment(audioProcessor.apvts, "LowCut Bypassed", lowcutBypassButton),
    highcutButtonAttachment(audioProcessor.apvts, "HighCut Bypassed", highcutBypassButton),
    peakButtonAttachment(audioProcessor.apvts, "Peak Bypassed", peakBypassButton),
    analyzerButtonAttachment(audioProcessor.apvts, "Analyzer Enabled", analyzerEnabledButton)


{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.


    //add label ranges?
    peakFreqSlider.labels.add({ 0.f, "20Hz" });
    peakFreqSlider.labels.add({ 1.f, "20kHz" });
    peakGainSlider.labels.add({ 0.f, "-24db" });
    peakGainSlider.labels.add({ 1.f, "+24db" });
    peakQualitySlider.labels.add({ 0.f, "0.1" });
    peakQualitySlider.labels.add({ 1.f, "10.0" });
    lowCutFreqSlider.labels.add({ 0.f, "20Hz" });
    lowCutFreqSlider.labels.add({ 1.f, "20kHz" });
    highCutFreqSlider.labels.add({ 0.f, "20Hz" });
    highCutFreqSlider.labels.add({ 1.f, "20kHz" });
    lowCutSlopeSlider.labels.add({ 0.f, "12" });
    lowCutSlopeSlider.labels.add({ 1.f, "48" });     
    highCutSlopeSlider.labels.add({ 0.f, "12" });
    highCutSlopeSlider.labels.add({ 1.f, "48" });

    //iterate through the vector of sliders we made 
    for (auto* comp : getComps()) 
    {
        addAndMakeVisible(comp);
    }

    peakBypassButton.setLookAndFeel(&lnf);
    lowcutBypassButton.setLookAndFeel(&lnf);
    highcutBypassButton.setLookAndFeel(&lnf);
    setSize(600, 480);
}

RomalEQAudioProcessorEditor::~RomalEQAudioProcessorEditor()
{

}






//==============================================================================
void RomalEQAudioProcessorEditor::paint (juce::Graphics& g) 
{

    using namespace juce;
    g.fillAll (Colours::black);

   
}

void RomalEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();

    float hRatio = 25.f / 100.f;//JUCE_LIVE_CONSTANT(25) / 100.f; //uncomment this to dial in psoitions

    //reserve top 1/3 for visualizer
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * hRatio);
    responseCurveComponent.setBounds(responseArea);
    bounds.removeFromTop(5); //give some room for sliders


    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);


    lowcutBypassButton.setBounds(lowCutArea.removeFromTop(25));
    highcutBypassButton.setBounds(highCutArea.removeFromTop(25));
    peakBypassButton.setBounds(bounds.removeFromTop(25));


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


std::vector<juce::Component*> RomalEQAudioProcessorEditor::getComps() {
    return{
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutSlopeSlider,
        &responseCurveComponent,
        &lowcutBypassButton, 
        &peakBypassButton, 
        &highcutBypassButton, 
        &analyzerEnabledButton
    };

}