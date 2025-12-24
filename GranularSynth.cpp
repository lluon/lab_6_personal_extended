#include "GranularSynth.h"
#include <Bela.h>
#include <libraries/sndfile/sndfile.h>
#include <algorithm>

GranularSynth::GranularSynth(float sampleRate, const std::string& filename, float grainSizeMs, float playbackSpeed, int maxGrains)
    : sampleRate(sampleRate), grainSizeMs(grainSizeMs), playbackSpeed(playbackSpeed), maxGrains(maxGrains)
{
    if (!loadAudioSample(filename)) {
        rt_printf("Failed to load %s\n", filename.c_str());
    }
    initializeGrains();
}

bool GranularSynth::loadAudioSample(const std::string& filename)
{
    SF_INFO info;
    SNDFILE* file = sf_open(filename.c_str(), SFM_READ, &info);
    if (!file) return false;
    audioSampleLength = info.frames;
    audioSample.resize(audioSampleLength);
    std::vector<float> buffer(audioSampleLength * info.channels);
    sf_readf_float(file, buffer.data(), audioSampleLength);
    for (int i = 0; i < audioSampleLength; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < info.channels; ++c) {
            sum += buffer[i * info.channels + c];
        }
        audioSample[i] = sum / info.channels;
    }
    sf_close(file);
    return true;
}

void GranularSynth::initializeGrains()
{
    grainSizeSamples = static_cast<int>(grainSizeMs * 0.001f * sampleRate);
    grains.resize(maxGrains);
    for (auto& g : grains) g.active = false;
}

void GranularSynth::triggerGrain(int position)
{
    for (auto& g : grains) {
        if (!g.active) {
            g.startSample = position;
            g.currentSample = static_cast<float>(position);
            g.speed = playbackSpeed;
            g.active = true;
            break;
        }
    }
}

float GranularSynth::processGrains()
{
    float output = 0.0f;
    int activeCount = 0;
    for (auto& g : grains) {
        if (g.active && g.currentSample < g.startSample + grainSizeSamples) {
            int idx = static_cast<int>(g.currentSample);
            float frac = g.currentSample - idx;
            if (idx >= 0 && idx + 1 < audioSampleLength) {
                float sample = (1.0f - frac) * audioSample[idx] + frac * audioSample[idx + 1];
                output += sample;
                ++activeCount;
            }
            g.currentSample += g.speed;
        } else {
            g.active = false;
        }
    }
    return activeCount > 0 ? output / activeCount : 0.0f;
}