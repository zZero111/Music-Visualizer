#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

// Parameter model. Must match VIS / SHAPES / NMOD in web/index.html.
struct SphereVisParam { const char* id; const char* name; float min, max, def; };
inline constexpr std::array<SphereVisParam, 11> kVisParams {{
    { "zoom",      "Zoom",         0.4f,  2.0f, 1.0f  },
    { "disp",      "Displacement", 0.0f,  1.0f, 0.2f  },
    { "detail",    "Detail",       0.3f,  6.0f, 1.5f  },
    { "flow",      "Flow Speed",   0.0f,  3.0f, 0.4f  },
    { "spin",      "Spin",        -2.0f,  2.0f, 0.15f },
    { "tilt",      "Tilt",        -1.5f,  1.5f, 0.2f  },
    { "twist",     "Twist",       -3.0f,  3.0f, 0.0f  },
    { "hue",       "Hue Shift",    0.0f,  1.0f, 0.0f  },
    { "glow",      "Glow",         0.0f,  2.0f, 0.5f  },
    { "wire",      "Wireframe",    0.0f,  1.0f, 0.3f  },
    { "particles", "Particles",    0.0f,  1.0f, 0.5f  },
}};

class SphereAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numMods = 8;

    SphereAudioProcessor();
    ~SphereAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Latest level (0..1) of each modifier's frequency range, same scale as the web preview.
    std::array<float, numMods> getModLevels() const;

    // Look settings that are not automatable (colours, logo, record settings), as JSON.
    juce::String getUiState() const;
    void setUiState (const juce::String&);

    // Records the plugin's input to a WAV file alongside a video recording.
    void startWavRecording (const juce::File&);
    void stopWavRecording();

    juce::AudioProcessorValueTreeState params;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void analyse();

    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, hopSize = 512;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann, false };
    std::array<float, fftSize> ring {};
    std::array<float, fftSize * 2> fftData {};
    int ringPos = 0, sinceLast = 0;
    double sr = 44100.0;
    std::array<std::atomic<float>, numMods> levels {};
    std::array<std::atomic<float>*, numMods> modLo {}, modHi {};

    juce::TimeSliceThread writerThread { "Sphere WAV writer" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SphereAudioProcessor)
};
