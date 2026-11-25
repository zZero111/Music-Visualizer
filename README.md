# Sphere Visualizer

An audio-reactive 3D visualizer you put on a track in your DAW.

- **6 shapes:** sphere, cube, diamond, torus, knot and a flat plane (2D terrain).
- **11 visual parameters:** zoom, displacement, detail, flow speed, spin, tilt, twist, hue shift, glow, wireframe and particles.
- **8 modifiers.** Each one follows a frequency range you set in Hz (or a steady LFO) and pushes one parameter up or down by an amount from -1 to +1. Drag a modifier chip onto a parameter to assign it.
- **25 factory presets**, unlimited saved presets (shared across projects) and auto-play through presets.
- **Custom colours** (core, edge, background) and a **logo overlay** with position, size and opacity.
- **Recording** in landscape 16:9, vertical 9:16 or square, at 720p, 1080p, 1440p or 4K and 30/60/120 fps, with an optional transparent background (WebM). In the plugin, videos are saved to `Movies/Sphere Visualizer` with a WAV of the track.
- **Performance settings:** mesh density and render scale.
- **Every parameter can be automated**, so DAW LFOs, automation lanes and MIDI controllers (through your DAW's MIDI learn) can move it.

## How it's put together

```
web/index.html      the visual and controls (three.js). Opens on its own as a browser preview,
                    and is also the plugin's window.
plugin/             JUCE 8 plugin (C++)
  Source/PluginProcessor.*   passes audio through untouched, runs an FFT, measures the 8 modifier ranges, holds the parameters, writes the WAV while recording
  Source/PluginEditor.*      shows web/index.html in a web view, sends band levels + parameters to it 30x/second,
                             and sends control changes back to the DAW
  Resources/three.min.js     bundled so the plugin works offline
.github/workflows/build.yml  builds Windows and Mac versions on GitHub automatically
```

Change the look in `web/index.html` and rebuild. The plugin uses the same file.

## Getting the plugin

**Easiest: let GitHub build it.** Push this folder to a GitHub repository. Under **Actions → Build plugin**, open the latest run and download `SphereVisualizer-Windows` or `SphereVisualizer-macOS`.

**Build it yourself:** install CMake 3.22+ and a compiler (Visual Studio 2022 on Windows, Xcode on Mac), then:

```
cmake -S plugin -B plugin/build -DCMAKE_BUILD_TYPE=Release
cmake --build plugin/build --config Release
```

The first configure downloads JUCE, which takes a couple of minutes.

On Windows, install Microsoft's WebView2 SDK once first (in PowerShell):

```
Register-PackageSource -ProviderName NuGet -Name nugetRepository -Location https://www.nuget.org/api/v2 -Force
Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.1901.177 -Source nugetRepository -Force
```

## Installing

- **Windows VST3:** copy `Sphere Visualizer.vst3` to `C:\Program Files\Common Files\VST3`
- **Mac VST3:** copy it to `~/Library/Audio/Plug-Ins/VST3`
- **Mac AU (Logic, GarageBand):** copy `Sphere Visualizer.component` to `~/Library/Audio/Plug-Ins/Components`
- **Standalone:** runs on its own and listens to your audio input

Rescan plugins in your DAW, then put Sphere Visualizer on any track or on the master. It doesn't change the sound.

Mac builds from GitHub are unsigned. If macOS blocks them, run `xattr -cr "<path to the plugin>"` once.

## Not done yet

- **Recording in the plugin** saves the video and the WAV as separate files: line them up at 0:00 in a video editor. The video starts when you press Record, not on the DAW's transport.
- **AAX (Pro Tools)** needs Avid's free AAX SDK and a signing account. Configure with `-DAAX_SDK_PATH=...` to add it.
