#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    std::vector<std::byte> toBytes (const juce::String& s)
    {
        const auto utf8 = s.toRawUTF8();
        const auto n = s.getNumBytesAsUTF8();
        return { reinterpret_cast<const std::byte*> (utf8), reinterpret_cast<const std::byte*> (utf8) + n };
    }

    juce::WebBrowserComponent::Options makeOptions()
    {
        using WBC = juce::WebBrowserComponent;
        return WBC::Options{}
            .withBackend (WBC::Options::Backend::webview2)
            .withWinWebView2Options (WBC::Options::WinWebView2{}
                .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("SphereVisualizer"))
                .withBackgroundColour (juce::Colour (0xff06070b)))
            .withNativeIntegrationEnabled()
            .withKeepPageLoadedWhenBrowserIsHidden();
    }
}

SphereAudioProcessorEditor::SphereAudioProcessorEditor (SphereAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      browser (makeOptions()
                 .withResourceProvider ([this] (const auto& url) { return getResource (url); })
                 .withEventListener ("param", [this] (const juce::var& v) { onParamFromPage (v); })
                 .withEventListener ("ready", [this] (const juce::var&) { onReady(); })
                 .withEventListener ("ui", [this] (const juce::var& v) { processor.setUiState (v.toString()); })
                 .withEventListener ("presets", [] (const juce::var& v) { presetsFile().replaceWithText (v.toString()); })
                 .withEventListener ("rec-start", [this] (const juce::var& v) { onRecStart (v); })
                 .withEventListener ("rec-chunk", [this] (const juce::var& v) { onRecChunk (v); })
                 .withEventListener ("rec-end", [this] (const juce::var&) { onRecEnd(); })
                 .withEventListener ("log", [] (const juce::var& v) { juce::Logger::outputDebugString ("[page] " + v.toString()); }))
{
    addAndMakeVisible (browser);
    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    setResizeLimits (640, 420, 3840, 2160);
    setSize (1100, 680);
    startTimerHz (30);
}

SphereAudioProcessorEditor::~SphereAudioProcessorEditor()
{
    stopTimer();
    if (videoOut != nullptr)
        onRecEnd();
}

void SphereAudioProcessorEditor::resized()
{
    browser.setBounds (getLocalBounds());
}

juce::File SphereAudioProcessorEditor::presetsFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Sphere Visualizer");
    dir.createDirectory();
    return dir.getChildFile ("user-presets.json");
}

juce::File SphereAudioProcessorEditor::recordingsFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userMoviesDirectory).getChildFile ("Sphere Visualizer");
    dir.createDirectory();
    return dir;
}

std::optional<juce::WebBrowserComponent::Resource> SphereAudioProcessorEditor::getResource (const juce::String& url)
{
    // Depending on the platform the provider is given a full URL or just a path ("/three.min.js").
    auto path = url.upToFirstOccurrenceOf ("?", false, false);
    if (path.contains ("://"))
        path = path.fromFirstOccurrenceOf ("://", false, false).fromFirstOccurrenceOf ("/", true, false);

    if (path.endsWith ("three.min.js"))
    {
        const auto* d = reinterpret_cast<const std::byte*> (BinaryData::three_min_js);
        return juce::WebBrowserComponent::Resource { { d, d + BinaryData::three_min_jsSize }, "text/javascript" };
    }

    if (path.isEmpty() || path == "/" || path.endsWith ("index.html"))
    {
        // web/index.html is written as a page fragment (the browser preview wraps it); wrap it here
        // and load three.js from inside the plugin instead of the CDN so it works offline.
        auto body = juce::String::fromUTF8 (BinaryData::index_html, BinaryData::index_htmlSize)
                        .replace ("https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js", "three.min.js");
        auto html = "<!doctype html><html><head><meta charset=\"utf-8\">"
                    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                    "<style>body{margin:0}</style>"
                    "<script>addEventListener('error',function(e){try{window.__JUCE__.backend.emitEvent('log',String(e.message)+' @'+e.lineno)}catch(x){}});</script>"
                    "</head><body>" + body + "</body></html>";
        return juce::WebBrowserComponent::Resource { toBytes (html), "text/html; charset=utf-8" };
    }

    return std::nullopt;
}

void SphereAudioProcessorEditor::onReady()
{
    pageReady = true;
    browser.emitEventIfBrowserIsVisible ("ui", processor.getUiState()); // empty for a new instance
    const auto presets = presetsFile();
    if (presets.existsAsFile())
        browser.emitEventIfBrowserIsVisible ("presets", presets.loadFileAsString());
}

void SphereAudioProcessorEditor::onParamFromPage (const juce::var& payload)
{
    const auto id = payload["id"].toString();
    if (auto* param = processor.params.getParameter (id))
    {
        const float norm = param->convertTo0to1 ((float) (double) payload["value"]);
        if (std::abs (norm - param->getValue()) < 1.0e-6f)
            return;
        param->beginChangeGesture();
        param->setValueNotifyingHost (norm);
        param->endChangeGesture();
    }
}

void SphereAudioProcessorEditor::onRecStart (const juce::var& payload)
{
    onRecEnd();
    const auto ext = payload["ext"].toString() == "mp4" ? "mp4" : "webm";
    const auto base = "Sphere " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
    videoFile = recordingsFolder().getChildFile (base + "." + ext);
    videoOut = videoFile.createOutputStream();
    processor.startWavRecording (recordingsFolder().getChildFile (base + ".wav"));
}

void SphereAudioProcessorEditor::onRecChunk (const juce::var& payload)
{
    if (videoOut == nullptr)
        return;
    juce::MemoryOutputStream decoded;
    if (juce::Base64::convertFromBase64 (decoded, payload["d"].toString()))
        videoOut->write (decoded.getData(), decoded.getDataSize());
}

void SphereAudioProcessorEditor::onRecEnd()
{
    processor.stopWavRecording();
    if (videoOut == nullptr)
        return;
    videoOut->flush();
    videoOut.reset();

    auto* result = new juce::DynamicObject();
    result->setProperty ("path", videoFile.getFullPathName());
    browser.emitEventIfBrowserIsVisible ("rec-saved", juce::var (result));
}

void SphereAudioProcessorEditor::timerCallback()
{
    if (! pageReady)
        return;

    juce::Array<juce::var> mods;
    for (auto l : processor.getModLevels())
        mods.add (l);

    auto* p = new juce::DynamicObject();
    for (auto* param : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            p->setProperty (ranged->getParameterID(), ranged->convertFrom0to1 (ranged->getValue()));

    auto* msg = new juce::DynamicObject();
    msg->setProperty ("b", mods);
    msg->setProperty ("p", juce::var (p));
    browser.emitEventIfBrowserIsVisible ("levels", juce::var (msg));
}
