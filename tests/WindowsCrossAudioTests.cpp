// Genuine output-device callback proof. No device-free mixer or manual clock.
// Run on an interactive desktop with an available audio device; failure is
// reported honestly rather than falling back to a synthetic mixer.
#include "AudioSystem.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

int main() {
    unsigned checks = 0, failures = 0;
    const auto check = [&](bool ok, const std::string& name) {
        ++checks;
        failures += !ok;
        std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
    };
    AudioSystem audio;
    std::string error;
    if (!audio.Init(error, false)) {
        check(false, "native output device opens: " + error);
        return 1;
    }
    check(audio.IsInitialized(), "native output device initialized without noDevice mode");
    AudioData data;
    data.sampleRate = 48000;
    data.channels = 1;
    data.samples.resize(48000 * 3);
    for (size_t i = 0; i < data.samples.size(); ++i)
        data.samples[i] = float(std::sin(double(i) * 6.283185307179586 * 330 / 48000)) * .1f;
    const auto clip = audio.CreateClip(std::move(data));
    AudioSettings settings;
    settings.spatial = false;
    settings.volume = .1f;
    const auto voice = audio.CreateVoice(clip, settings, error);
    check(clip.IsValid() && voice.IsValid(), "ordinary buffered voice is created: " + error);
    check(audio.Play(voice), "ordinary voice starts on the native device");
    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    AudioVoiceSnapshot first, paused, resumed;
    check(audio.Snapshot(voice, first) && first.cursorAvailable && first.cursorFrames > 0,
          "actual output callback consumes media frames (no manual mixer advancement)");
    check(audio.Pause(voice), "native voice pauses");
    audio.Snapshot(voice, paused);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    AudioVoiceSnapshot held;
    check(audio.Snapshot(voice, held) && held.cursorFrames == paused.cursorFrames,
          "paused native voice retains its media cursor");
    check(audio.Resume(voice), "native voice resumes");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    check(audio.Snapshot(voice, resumed) && resumed.cursorFrames > held.cursorFrames,
          "resumed native callback advances the real cursor");
    check(audio.Stop(voice), "native voice stops cleanly");
    std::cout << "CURSOR_FRAMES " << first.cursorFrames << ' ' << paused.cursorFrames << ' '
              << resumed.cursorFrames << '\n';
    audio.DestroyVoice(voice);
    audio.DestroyClip(clip);
    audio.Shutdown();
    std::cout << "SUMMARY " << checks << " checks " << failures << " failures\n";
    return failures ? 1 : 0;
}
