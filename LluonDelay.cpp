#include "LluonDelay.h"
#include <algorithm>
#include <cmath>
#include <cstring> // for memset if needed

LluonDelay::LluonDelay(int sampleRate, int maxDelaySamples)
    : sr(sampleRate), maxDelay(maxDelaySamples)
{
    buffer = new float[maxDelaySamples];
    std::fill(buffer, buffer + maxDelaySamples, 0.0f);
}

LluonDelay::~LluonDelay()
{
    delete[] buffer;
}

void LluonDelay::setTime(float ms)
{
    delaySamples = std::min(static_cast<int>(ms * sr / 1000.0f), maxDelay - 1);
}

void LluonDelay::setFeedback(float f)
{
    feedback = std::max(0.0f, std::min(0.99f, f));
}

void LluonDelay::setDiffusion(float d)
{
    diffusion = std::max(0.0f, std::min(1.0f, d));
}

float LluonDelay::process(float input)
{
    int readIndex = (writeIndex - delaySamples + maxDelay) % maxDelay;
    float delayed = buffer[readIndex];
    float out = input * diffusion + delayed * (1.0f - diffusion);
    buffer[writeIndex] = input + delayed * feedback;
    writeIndex = (writeIndex + 1) % maxDelay;
    return out;
}