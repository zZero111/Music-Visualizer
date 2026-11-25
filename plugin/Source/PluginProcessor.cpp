#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Same dB window as the web preview's AnalyserNode (minDecibels -90, maxDecibels -10).
    constexpr float minDb = -90.0f, maxDb = -10.0f;

    juce::String modId (int i, const char* field) { return "m" + juce::String (i) + "_" + field; }
}

SphereAudioProcessor::SphereAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "SphereState", createLayout())
{
    for (auto& l : levels) l.store (0.0f);
    for (int i = 0; i < numMods; ++i)
    {
        modLo[(size_t) i] = params.getRawParameterValue (modId (i + 1, "lo"));
        modHi[(size_t) i] = params.getRawParameterValue (modId (i + 1, "hi"));
    }
    writerThread.startThread();
}

SphereAudioProcessor::~SphereAudioProcessor()
{
    stopWavRecording();
    writerThread.stopThread (2000);
}

juce::AudioProcessorValueTreeState::ParameterLayout SphereAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray targets;
    for (const auto& v : kVisParams)
    {
        targets.add (v.name);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { v.id, 1 }, v.name, NormalisableRange<float> (v.min, v.max), v.def));
    }
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "shape", 1 }, "Shape",
                    StringArray { "Sphere", "Cube", "Diamond", "Torus", "Knot", "Plane" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "density", 1 }, "Mesh Density",
                    StringArray { "Low", "Medium", "High", "Ultra" }, 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rscale", 1 }, "Render Scale",
                    StringArray { "50%", "75%", "100%", "150%" }, 2));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sens", 1 }, "Sensitivity", NormalisableRange<float> (0.2f, 3.0f), 1.2f));

    NormalisableRange<float> hz (20.0f, 20000.0f);
    hz.setSkewForCentre (1000.0f);
    for (int i = 1; i <= numMods; ++i)
    {
        const String n = "M" + String (i) + " ";
        layout.add (std::make_unique<AudioParameterBool>   (ParameterID { modId (i, "on"), 1 },   n + "On", false));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { modId (i, "src"), 1 },  n + "Source", StringArray { "Audio", "LFO" }, 0));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { modId (i, "lo"), 1 },   n + "Low Hz", hz, 60.0f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { modId (i, "hi"), 1 },   n + "High Hz", hz, 150.0f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { modId (i, "rate"), 1 }, n + "LFO Rate", NormalisableRange<float> (0.02f, 8.0f, 0.0f, 0.4f), 0.5f));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { modId (i, "tgt"), 1 },  n + "Target", targets, 0));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { modId (i, "amt"), 1 },  n + "Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.5f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { modId (i, "bend"), 1 }, n + "Curve", NormalisableRange<float> (-1.0f, 1.0f), 0.3f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { modId (i, "thr"), 1 },  n + "Threshold", NormalisableRange<float> (0.0f, 0.9f), 0.05f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { modId (i, "att"), 1 },  n + "Attack ms", NormalisableRange<float> (1.0f, 500.0f, 0.0f, 0.5f), 10.0f));
        layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { modId (i, "rel"), 1 },  n + "Release ms", NormalisableRange<float> (10.0f, 2000.0f, 0.0f, 0.5f), 180.0f));
    }
    return layout;
}

bool SphereAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SphereAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    ring.fill (0.0f);
    ringPos = sinceLast = 0;
}

void SphereAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    int state = -1;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            state = pos->getIsPlaying() ? 1 : 0;
    transport.store (state);
    const int chans = getTotalNumInputChannels();
    for (int i = chans; i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    {
        const juce::ScopedTryLock sl (writerLock);
        if (sl.isLocked())
            if (auto* w = activeWriter.load())
                w->write (buffer.getArrayOfReadPointers(), buffer.getNumSamples());
    }

    // Audio passes through untouched; we only listen.
    const float scale = chans > 0 ? 1.0f / (float) chans : 0.0f;
    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        float mono = 0.0f;
        for (int c = 0; c < chans; ++c)
            mono += buffer.getSample (c, s);
        ring[(size_t) ringPos] = mono * scale;
        ringPos = (ringPos + 1) % fftSize;

        if (++sinceLast >= hopSize)
        {
            sinceLast = 0;
            analyse();
        }
    }
}

void SphereAudioProcessor::analyse()
{
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float binHz = (float) sr / (float) fftSize;
    const int maxBin = fftSize / 2 - 1;
    for (int m = 0; m < numMods; ++m)
    {
        const float a = modLo[(size_t) m]->load(), b = modHi[(size_t) m]->load();
        const int lo = juce::jlimit (1, maxBin, (int) std::floor (juce::jmin (a, b) / binHz));
        const int hi = juce::jlimit (lo, maxBin, (int) std::ceil (juce::jmax (a, b) / binHz));
        float sum = 0.0f;
        for (int k = lo; k <= hi; ++k)
        {
            // Hann window halves the amplitude; scale back so a 0 dBFS sine reads about 0 dB.
            const float mag = fftData[(size_t) k] * 4.0f / (float) fftSize;
            const float db = juce::Decibels::gainToDecibels (mag, -200.0f);
            sum += juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        }
        levels[(size_t) m].store (sum / (float) (hi - lo + 1));
    }
}

std::array<float, SphereAudioProcessor::numMods> SphereAudioProcessor::getModLevels() const
{
    std::array<float, numMods> out {};
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = levels[i].load();
    return out;
}

juce::String SphereAudioProcessor::getUiState() const
{
    return params.state.getProperty ("ui").toString();
}

void SphereAudioProcessor::setUiState (const juce::String& json)
{
    params.state.setProperty ("ui", json, nullptr);
}

void SphereAudioProcessor::startWavRecording (const juce::File& file)
{
    stopWavRecording();
    if (sr <= 0)
        return;

    file.deleteFile();
    if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
    {
        juce::WavAudioFormat wav;
        const int chans = juce::jmax (1, getTotalNumInputChannels());
        if (auto* writer = wav.createWriterFor (stream.get(), sr, (unsigned int) chans, 24, {}, 0))
        {
            stream.release(); // the writer owns it now
            threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer, writerThread, 32768);
            const juce::ScopedLock sl (writerLock);
            activeWriter = threadedWriter.get();
        }
    }
}

void SphereAudioProcessor::stopWavRecording()
{
    {
        const juce::ScopedLock sl (writerLock);
        activeWriter = nullptr;
    }
    threadedWriter.reset(); // flushes and closes the file
}

void SphereAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = params.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SphereAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (params.state.getType()))
            params.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SphereAudioProcessor::createEditor()
{
    return new SphereAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SphereAudioProcessor();
}
