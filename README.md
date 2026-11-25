# Sphere Visualizer

An audio-reactive 3D visualizer you put on a track in your DAW.

- **5 mixable styles:** Nova (fluffy shells, spike strands and wireframe shards), Ribbons (silky bands with a fine mesh), Mesh (dense spiky wireframe), Burst (radial explosion) and Plasma (burning star surface). Each has a level knob, so you can blend them, and modifiers can crossfade them with the music. Everything is finished with bloom and filmic tone mapping.
- **6 shapes:** sphere, cube, diamond, torus, knot and a flat plane (2D terrain).
- **20 knobs:** five style levels, then zoom, displace, spikes, detail, twist, flow, spin, tilt, fluff, shards, particles, glow, brightness, hue and saturation. Orange rings on each knob show live modulation.
- **8 modifiers.** Each one follows a frequency range you set in Hz (or an LFO) and pushes one knob up or down. Drag a modifier from the dock onto a knob to assign it.
- **Response curves:** every modifier has a drawable curve (drag sideways for the threshold, up and down for the shape), Linear / Punchy / Smooth / Gate presets, and attack and release times.
- **Colour:** core, glow, tips and background colours, 14 palettes, Randomize, and colour modes (Static, Cycle, Spectrum, Reactive).
- **25 factory presets**, unlimited saved presets (shared across projects) and auto-play through presets.
- **Logo overlay** with position, size and opacity.
- **Render song to video** (preview with a loaded file, or the standalone app): frame-by-frame, hardware-encoded MP4 with audio; no dropped frames at any resolution.
- **Record live** auto-plays: in the preview, Record starts your track from the top and stops when it ends; in the plugin, Record arms and then starts and stops with your DAW's transport. Formats: 16:9, 9:16 or 1:1, at 720p to 4K and 30/60/120 fps, with an optional transparent background (WebM). In the plugin, videos are saved to `Movies/Sphere Visualizer` with a WAV of the track.
- **Every knob and modifier setting can be automated**, so DAW LFOs, automation lanes and MIDI controllers (through your DAW's MIDI learn) can move it.

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
- **Standalone app** (`Sphere Visualizer.exe` / `Sphere Visualizer.app`): runs without a DAW. Use **Load song** to open a WAV, MP3, Opus, AIFF, FLAC or Ogg file. On the Export tab, **Render song to video** draws every frame at exact time steps and encodes with your graphics card (WebCodecs), so 4K 60 never stutters, and saves one MP4 with the audio inside to `Videos/Sphere Visualizer`. **Record live** captures in real time instead (video plus a separate WAV). Without a song it reacts to your audio input (pick it under Options; Windows' Stereo Mix or a virtual cable lets it hear anything your PC plays).

Rescan plugins in your DAW, then put Sphere Visualizer on any track or on the master. It doesn't change the sound.

Mac builds from GitHub are unsigned. If macOS blocks them, run `xattr -cr "<path to the plugin>"` once.

## Not done yet

- **Recording in the plugin** saves the video and the WAV as separate files: line them up at 0:00 in a video editor. The video starts when you press Record, not on the DAW's transport.
- **AAX (Pro Tools)** needs Avid's free AAX SDK and a signing account. Configure with `-DAAX_SDK_PATH=...` to add it.
