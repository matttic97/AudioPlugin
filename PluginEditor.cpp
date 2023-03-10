#include "PluginProcessor.h"
#include "PluginEditor.h"


void LookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float sliderPosProportional,
                                   float rotaryStartAngle, float rotaryEndAngle, juce::Slider &slider)
{
    using namespace juce;

    auto bounds = Rectangle<float>(x, y, width, height);

    g.setColour(slider.isEnabled() ? Colour(0, 102, 204) : Colours::darkgrey);
    g.fillEllipse(bounds);

    g.setColour(slider.isEnabled() ? Colour(0, 76, 153) : Colours::dimgrey);
    g.drawEllipse(bounds, 2.f);

    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();

        Path p;

        Rectangle<float> r;
        r.setLeft(center.getX() - 2);
        r.setRight(center.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5);

        p.addRoundedRectangle(r, 2.f);

        jassert(rotaryStartAngle < rotaryEndAngle);

        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);

        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));

        g.fillPath(p);

        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);

        r.setSize(strWidth + 4, rswl->getTextHeight() + 2);
        r.setCentre(bounds.getCentre());

        g.setColour(slider.isEnabled() ? Colours::black : Colours::darkgrey);
        g.fillRect(r);

        g.setColour(slider.isEnabled() ? Colours::white : Colours::lightgrey);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

void LookAndFeel::drawToggleButton(juce::Graphics &g, juce::ToggleButton &toggleButton, bool shouldDrawButtonHighlighted,
                                   bool shouldDrawButtonAsDown)
{
    using namespace juce;

    Path powerButton;

    auto bounds = toggleButton.getLocalBounds();
    auto size = jmin (bounds.getWidth(), bounds.getHeight()) - 6;
    auto r = bounds.withSizeKeepingCentre(size, size).toFloat();

    float ang = 30.f;
    size -= 6;

    powerButton.addCentredArc(r.getCentreX(),
                              r.getCentreY(),
                              size * 0.5,
                              size * 0.5,
                              0,
                              degreesToRadians(ang),
                              degreesToRadians(360 - ang),
                              true);

    powerButton.startNewSubPath(r.getCentreX(), r.getY());
    powerButton.lineTo(r.getCentre());

    PathStrokeType pst (2, PathStrokeType::JointStyle::curved);

    auto color = toggleButton.getToggleState() ? Colours::dimgrey : Colour(0, 172, 1);

    g.setColour(color);
    g.strokePath(powerButton, pst);
    g.drawEllipse(r, 2);
}

//==============================================================================
void RotarySliderWithLabels::paint(juce::Graphics &g)
{
    using namespace juce;

    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;

    auto range = getRange();

    auto sliderBounds = getSliderBounds();

//    g.setColour(Colours::red);
//    g.drawRect(getLocalBounds());
//    g.setColour(Colours::yellow);
//    g.drawRect(sliderBounds);

    getLookAndFeel().drawRotarySlider(
            g,
            sliderBounds.getX(),
            sliderBounds.getY(),
            sliderBounds.getWidth(),
            sliderBounds.getHeight(),
            jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
            startAng,
            endAng,
            *this);

    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;

    g.setColour(isEnabled() ? Colour(76, 153, 0) : Colours::darkgrey);
    g.setFont(getTextHeight());

    auto numChoices = labels.size();
    for (int i = 0; i < numChoices; ++i)
    {
        auto pos = labels[i].position;
        jassert(0.f <= pos);
        jassert(pos <= 1.f);

        auto ang = jmap(pos, 0.0f, 1.f, startAng, endAng);

        auto c = center.getPointOnCircumference(radius + getTextHeight() * 0.5f + 1, ang);

        Rectangle<float> r;
        auto str = labels[i].label;
        r.setSize(g.getCurrentFont().getStringWidth(str), getTextHeight());
        r.setCentre(c);
        r.setY(r.getY() + getTextHeight());

        g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight()) - getTextHeight() * 2;

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
    bool suff_dbOrQ = suffix.isEmpty() || suffix == "dB";
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float value = getValue();
        if (value >= 1000.f)
        {
            value /= 1000.f;
            addK = true;
        }

        str = juce::String(value, (addK || suff_dbOrQ ? 2 : 0));
    }
    else
    {
        jassertfalse;
    }

    if (suffix.isNotEmpty())
    {
        str << " ";
        if (addK)
            str << "k";
        str << suffix;
    }

    return str;
}

//==============================================================================
ResponseCurveComponent::ResponseCurveComponent(AudioPluginAudioProcessor& p) : processorRef(p),
leftPathProducer(processorRef.leftChannelFifo),
rightPathProducer(processorRef.rightChannelFifo)
{
    const auto& params = processorRef.getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        params[i]->addListener(this);
    }

    updateChain();

    startTimerHz(60);
}

ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = processorRef.getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        params[i]->removeListener(this);
    }
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void PathProducer::process(juce::Rectangle<float> fftBounds, double sampleRate)
{
    juce::AudioBuffer<float> tempIncomingBuffer;

    while (channelFifo->getNumCompleteBuffersAvailable() > 0)
    {
        if (channelFifo->getAudioBuffer(tempIncomingBuffer))
        {
            auto size = tempIncomingBuffer.getNumSamples();

            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0),
                                              monoBuffer.getReadPointer(0, size),
                                              monoBuffer.getNumSamples() - size);

            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size),
                                              tempIncomingBuffer.getReadPointer(0, 0),
                                              size);

            channelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f);
        }
    }

    const auto fftSize = channelFFTDataGenerator.getFFTSize();
    const auto binWidth = sampleRate / (double)fftSize;

    while (channelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0)
    {
        std::vector<float> fftData;
        if (channelFFTDataGenerator.getFFTData(fftData))
        {
            pathProducer.generatePath(fftData, fftBounds, fftSize, binWidth, -48.f);
        }
    }

    while (pathProducer.getNumPathsAvailable())
    {
        pathProducer.getPath(channelFFTPath);
    }
}

void ResponseCurveComponent::timerCallback()
{

    auto fftBounds = getAnalysisArea().toFloat();
    auto sampleRate = processorRef.getSampleRate();

    leftPathProducer.process(fftBounds, sampleRate);
    rightPathProducer.process(fftBounds, sampleRate);

    if (parametersChanged.compareAndSetBool(false, true))
    {
        updateChain();
    }

    repaint();
}

void ResponseCurveComponent::updateChain()
{
    auto chainSettings = getChainSettings(processorRef.apvts);

    monoChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    monoChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);

    auto peakCoefficients = makePeakFilter(chainSettings, processorRef.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);

    auto lowCutCoefficients = makeLowCutFilter(chainSettings, processorRef.getSampleRate());
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);

    auto highCutCoefficients = makeHighCutFilter(chainSettings, processorRef.getSampleRate());
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);
}

void ResponseCurveComponent::paint (juce::Graphics& g)
{
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colours::black);

    g.drawImage(background, getLocalBounds().toFloat());

    auto responseArea = getAnalysisArea();

    auto w = responseArea.getWidth();

    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();

    auto sampleRate = processorRef.getSampleRate();

    std::vector<double> mags;
    mags.resize(w);
    for (int i = 0; i < w; ++i)
    {
        double mag = 1.f;
        auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);

        if (!monoChain.isBypassed<ChainPositions::Peak>())
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (!monoChain.isBypassed<ChainPositions::LowCut>())
        {
            if (!lowCut.isBypassed<0>())
                mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowCut.isBypassed<1>())
                mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowCut.isBypassed<2>())
                mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowCut.isBypassed<3>())
                mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        if (!monoChain.isBypassed<ChainPositions::HighCut>()) {
            if (!highCut.isBypassed<0>())
                mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highCut.isBypassed<1>())
                mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highCut.isBypassed<2>())
                mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highCut.isBypassed<3>())
                mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }

        mags[i] = Decibels::gainToDecibels(mag);
    }

    Path responseCurve;

    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input)
    {
        return jmap (input, -24.0, 24.0, outputMin, outputMax);
    };

    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));

    for (size_t i = 1; i < mags.size(); ++i)
    {
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
    }


    auto leftChannelFFTPath = leftPathProducer.getPath();

    leftChannelFFTPath.applyTransform(AffineTransform().translated(responseArea.getX(), responseArea.getY()));

    g.setColour(Colours::yellow);
    g.strokePath(leftChannelFFTPath, PathStrokeType(2));

    auto rightChannelFFTPath = rightPathProducer.getPath();

    rightChannelFFTPath.applyTransform(AffineTransform().translated(responseArea.getX(), responseArea.getY()));

    g.setColour(Colours::yellowgreen);
    g.strokePath(rightChannelFFTPath, PathStrokeType(2));

    g.setColour(Colours::orange);
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.f, 1.f);

    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.f));
}

void ResponseCurveComponent::resized()
{
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);

    Graphics g(background);
    auto fontHeight = 10;
    g.setFont(fontHeight);

    auto renderArea = getAnalysisArea();
    auto left = renderArea.getX();
    auto right = renderArea.getRight();
    auto top = renderArea.getY();
    auto bottom = renderArea.getBottom();
    auto width = renderArea.getWidth();

    Array<float> freqs
    {
        50, 100,
        200, 500, 1000,
        2000, 5000, 10000
    };
    for (auto f : freqs)
    {
        auto normX = mapFromLog10(f, 20.f, 20000.f);
        auto x = left + width * normX;
        g.setColour(Colours::dimgrey);
        g.drawVerticalLine(x, top, bottom);

        String str;
        if (f >= 1000)
            str << f/1000 << "kHz";
        else
            str << f << "Hz";

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setCentre(x, 0);
        r.setY(2);

        g.setColour(Colours::white);
        g.drawFittedText(str, r, Justification::centred, 1);
    }

    const int paddingX = 4;
    Array<float> gains
    {
        -24, -12, 0, 12, 24
    };
    for (auto gain : gains)
    {
        auto y = jmap(gain, -24.f, 24.f, float(bottom), float(top));
        g.setColour(gain == 0.f ? Colours::darkgrey : Colours::dimgrey);
        g.drawHorizontalLine(y, left, right);

        String str;
        str << (gain - 24.f);

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setX(paddingX);
        r.setCentre(r.getCentreX(), y);

        g.setColour(Colours::white);
        g.drawFittedText(str, r, Justification::centred, 1);

        str.clear();
        if (gain > 0)
            str << "+";
        str << gain;

        textWidth = g.getCurrentFont().getStringWidth(str);
        r.setSize(textWidth, fontHeight);
        r.setX(getWidth() - textWidth - paddingX);
        r.setCentre(r.getCentreX(), y);
        g.drawFittedText(str, r, Justification::centred, 1);
    }
}

juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop(15);
    bounds.removeFromBottom(2);
    bounds.removeFromLeft(25);
    bounds.removeFromRight(25);

    return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea()
{
    auto bounds = getRenderArea();

    bounds.removeFromTop(5);
    bounds.removeFromBottom(5);

    return bounds;
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      peakFreqSlider(*processorRef.apvts.getParameter("Peak Freq"), "Hz"),
      peakGainSlider(*processorRef.apvts.getParameter("Peak Gain"), "dB"),
      peakQSlider(*processorRef.apvts.getParameter("Peak Q"), ""),
      lowCutFreqSlider(*processorRef.apvts.getParameter("LowCut Freq"), "Hz"),
      lowCutSlopeSlider(*processorRef.apvts.getParameter("LowCut Slope"), "dB/Oct"),
      highCutFreqSlider(*processorRef.apvts.getParameter("HighCut Freq"), "Hz"),
      highCutSlopeSlider(*processorRef.apvts.getParameter("HighCut Slope"), "dB/Oct"),

      responseCurveComponent(processorRef),
      peakFreqSliderAttachment(processorRef.apvts, "Peak Freq", peakFreqSlider),
      peakGainSliderAttachment(processorRef.apvts, "Peak Gain", peakGainSlider),
      peakQSliderAttachment(processorRef.apvts, "Peak Q", peakQSlider),
      lowCutFreqSliderAttachment(processorRef.apvts, "LowCut Freq", lowCutFreqSlider),
      lowCutSlopeSliderAttachment(processorRef.apvts, "LowCut Slope", lowCutSlopeSlider),
      highCutFreqSliderAttachment(processorRef.apvts, "HighCut Freq", highCutFreqSlider),
      highCutSlopeSliderAttachment(processorRef.apvts, "HighCut Slope", highCutSlopeSlider),

      lowCutBypassButtonAttachment(processorRef.apvts, "LowCut Bypassed", lowCutBypassButton),
      peakBypassButtonAttachment(processorRef.apvts, "Peak Bypassed", peakBypassButton),
      highCutBypassButtonAttachment(processorRef.apvts, "HighCut Bypassed", highCutBypassButton),
      analyzerEnableButtonAttachment(processorRef.apvts, "Analyzer Enabled", analyzerEnableButton)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

    peakFreqSlider.labels.add({0.f, "20Hz"});
    peakFreqSlider.labels.add({1.f, "20kHz"});
    peakGainSlider.labels.add({0.f, "-24dB"});
    peakGainSlider.labels.add({1.f, "+24dB"});
    peakQSlider.labels.add({0.f, "0.1"});
    peakQSlider.labels.add({1.f, "10"});

    lowCutFreqSlider.labels.add({0.f, "20Hz"});
    lowCutFreqSlider.labels.add({1.f, "1kHz"});
    lowCutSlopeSlider.labels.add({0.f, "12"});
    lowCutSlopeSlider.labels.add({1.f, "48"});

    highCutFreqSlider.labels.add({0.f, "20kHz"});
    highCutFreqSlider.labels.add({1.f, "1kHz"});
    highCutSlopeSlider.labels.add({0.f, "12"});
    highCutSlopeSlider.labels.add({1.f, "48"});

    for (auto* comp : getComponents())
    {
        addAndMakeVisible(comp);
    }

    lowCutBypassButton.setLookAndFeel(&lnf);
    peakBypassButton.setLookAndFeel(&lnf);
    highCutBypassButton.setLookAndFeel(&lnf);

    auto safePtr = juce::Component::SafePointer<AudioPluginAudioProcessorEditor>(this);

    this->updateLowCutSliders(safePtr);
    this->updatePeakSliders(safePtr);
    this->updateHighCutSliders(safePtr);

    lowCutBypassButton.onClick = [this, safePtr] (){ this->updateLowCutSliders(safePtr); };
    peakBypassButton.onClick = [this, safePtr] () { this->updatePeakSliders(safePtr); };
    highCutBypassButton.onClick = [this, safePtr] () { this->updateHighCutSliders(safePtr); };

    setSize (700, 600);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    lowCutBypassButton.setLookAndFeel(nullptr);
    peakBypassButton.setLookAndFeel(nullptr);
    highCutBypassButton.setLookAndFeel(nullptr);
}

void AudioPluginAudioProcessorEditor::updateLowCutSliders(juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safePtr)
{
    if (auto* comp = safePtr.getComponent())
    {
        auto bypassed = comp->lowCutBypassButton.getToggleState();

        comp->lowCutFreqSlider.setEnabled(!bypassed);
        comp->lowCutSlopeSlider.setEnabled(!bypassed);
    }
}

void AudioPluginAudioProcessorEditor::updatePeakSliders(juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safePtr)
{
    if (auto* comp = safePtr.getComponent())
    {
        auto bypassed = comp->peakBypassButton.getToggleState();

        comp->peakFreqSlider.setEnabled(!bypassed);
        comp->peakGainSlider.setEnabled(!bypassed);
        comp->peakQSlider.setEnabled(!bypassed);
    }
}

void AudioPluginAudioProcessorEditor::updateHighCutSliders(juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safePtr)
{
    if (auto* comp = safePtr.getComponent())
    {
        auto bypassed = comp->highCutBypassButton.getToggleState();

        comp->highCutFreqSlider.setEnabled(!bypassed);
        comp->highCutSlopeSlider.setEnabled(!bypassed);
    }
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colours::black);
}

void AudioPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    auto bounds = getLocalBounds();
    float hRatio = 30.f / 100.f;
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * hRatio);

    responseCurveComponent.setBounds(responseArea);

    bounds.removeFromTop(5);

    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    lowCutBypassButton.setBounds(lowCutArea.removeFromTop(25));
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);

    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);
    highCutBypassButton.setBounds(highCutArea.removeFromTop(25));
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);

    peakBypassButton.setBounds(bounds.removeFromTop(25));
    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQSlider.setBounds(bounds);
}

//==============================================================================
std::vector<juce::Component*> AudioPluginAudioProcessorEditor::getComponents()
{
    return {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQSlider,
        &lowCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutFreqSlider,
        &highCutSlopeSlider,
        &responseCurveComponent,
        &lowCutBypassButton,
        &peakBypassButton,
        &highCutBypassButton,
        &analyzerEnableButton
    };
}