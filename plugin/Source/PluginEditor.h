#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// The editor is a web view showing web/index.html (the same page as the browser preview).
// A 30 Hz timer pushes modifier levels and parameter values to the page; the page sends
// parameter changes back, which go to the host so they can be automated and MIDI-mapped.
// It also saves user presets to disk and writes video recordings the page streams over.
class SphereAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit SphereAudioProcessorEditor (SphereAudioProcessor&);
    ~SphereAudioProcessorEditor() override;

    void resized() override;

private:
    void timerCallback() override;
    void onParamFromPage (const juce::var& payload);
    void onReady();
    void onRecStart (const juce::var& payload);
    void onRecChunk (const juce::var& payload);
    void onRecEnd();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    static juce::File presetsFile();
    static juce::File recordingsFolder();

    SphereAudioProcessor& processor;
    juce::WebBrowserComponent browser;
    bool pageReady = false;

    std::unique_ptr<juce::FileOutputStream> videoOut;
    juce::File videoFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SphereAudioProcessorEditor)
};
