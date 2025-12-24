#pragma once

class LluonDelay {
public:
    LluonDelay(int sampleRate, int maxDelaySamples);
    ~LluonDelay();
    void setTime(float ms);
    void setFeedback(float f);
    void setDiffusion(float d);
    float process(float input);
private:
    int sr;
    int maxDelay;
    int delaySamples = 0;
    int writeIndex = 0;
    float feedback = 0.0f;
    float diffusion = 0.0f;
    float* buffer = nullptr;
};