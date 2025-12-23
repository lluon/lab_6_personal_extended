#pragma once

class LluonDelay {
public:
    LluonDelay(int sampleRate, int maxDelaySamples = 132300);
    ~LluonDelay();

    void setTime(float ms);
    void setFeedback(float f);
    void setDiffusion(float d);
    float process(float input);

private:
    int sr;
    int maxDelay;
    float* buffer = nullptr;
    int writeIndex = 0;
    int delaySamples = 1000;
    float feedback = 0.6f;
    float diffusion = 0.25f;
};