#pragma once
#include <vector>
#include <string>

struct Grain {
    bool active = false;
    int startSample = 0;
    float currentSample = 0.0f;
    float speed = 1.0f;
};

class GranularSynth {
public:
    GranularSynth(float sampleRate, const std::string& filename, float grainSizeMs, float playbackSpeed, int maxGrains);
    void triggerGrain(int position);
    float processGrains();
    int getAudioSampleLength() const { return audioSampleLength; }
    int getGrainSizeSamples() const { return grainSizeSamples; }
private:
    bool loadAudioSample(const std::string& filename);
    void initializeGrains();
    float sampleRate;
    float grainSizeMs;
    float playbackSpeed;
    int maxGrains;
    std::vector<float> audioSample;
    int audioSampleLength = 0;
    int grainSizeSamples = 0;
    std::vector<Grain> grains;
};