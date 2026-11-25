#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

// Reads .opus files (Ogg Opus) with Xiph's opusfile. Opus always decodes at 48 kHz;
// the player resamples to the device rate.
class OpusAudioFormat : public juce::AudioFormat
{
public:
    OpusAudioFormat();

    juce::Array<int> getPossibleSampleRates() override { return { 48000 }; }
    juce::Array<int> getPossibleBitDepths() override { return { 32 }; }
    bool canDoStereo() override { return true; }
    bool canDoMono() override { return true; }

    juce::AudioFormatReader* createReaderFor (juce::InputStream* source, bool deleteStreamIfOpeningFails) override;

    juce::AudioFormatWriter* createWriterFor (juce::OutputStream*, double, unsigned int, int,
                                              const juce::StringPairArray&, int) override
    {
        return nullptr; // decoding only
    }
};
