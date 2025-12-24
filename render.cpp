#include <Bela.h>
#include <libraries/Trill/Trill.h>
#include <unistd.h> // for usleep()
#include <cmath>
#include <cstdlib>
#include "GranularSynth.h"
#include "LluonDelay.h"

// Global objects
GranularSynth* granular = nullptr;
LluonDelay* delay = nullptr;
Trill trill;

// Touch values (updated in background task)
float touchX = 0.5f;
float touchY = 0.5f;
float touchSize = 0.0f;

// Granular control
float scanPosition = 0.0f;
float grainTimer = 0.0f;
const float baseGrainInterval = 0.09f;

// Background task to read Trill continuously
void readTrillLoop(void*)
{
    while (!Bela_stopRequested()) {
        trill.readI2C();
        float x = trill.compoundTouchHorizontalLocation();
        float y = trill.compoundTouchLocation();
        float size = trill.compoundTouchSize();
        if (x >= 0.0f) touchX = x;
        if (y >= 0.0f) touchY = y;
        if (size >= 0.0f) touchSize = size;
        usleep(5000); // ~200 Hz update rate
    }
}

bool setup(BelaContext* context, void* userData)
{
    srand(123);

    granular = new GranularSynth(context->audioSampleRate, "Ornella.wav", 140.0f, 1.0f, 50);
    if (granular->getAudioSampleLength() == 0) {
        return false;  // Silent fail if audio file missing
    }

    delay = new LluonDelay(static_cast<int>(context->audioSampleRate),
                           3 * static_cast<int>(context->audioSampleRate));

    // Initialise Trill Square at its default address 0x28
    if (trill.setup(1, Trill::SQUARE, 0x28) != 0) {
        // If no Trill detected, continue with default touch values (no interaction)
    } else {
        trill.setMode(Trill::CENTROID);
        trill.setNoiseThreshold(0.06f);
        trill.setScanSettings(3, 12);
    }

    // Schedule background task
    AuxiliaryTask trillTask = Bela_createAuxiliaryTask(readTrillLoop, 90, "trill-reader");
    Bela_scheduleAuxiliaryTask(trillTask);

    return true;
}

void render(BelaContext* context, void* userData)
{
    int sampleLength = granular->getAudioSampleLength();
    int grainSizeSamples = granular->getGrainSizeSamples();
    float sr = context->audioSampleRate;

    for (unsigned int n = 0; n < context->audioFrames; n++) {
        bool touched = (touchSize > 0.02f);

        float speedFactor = touched ? (0.1f + touchX * 0.6f) : 0.05f;

        float density = touched ? (1.0f + touchSize * 5.0f) : 0.3f;
        float currentInterval = baseGrainInterval / density;

        grainTimer += 1.0f / sr;
        if (grainTimer >= currentInterval) {
            grainTimer -= currentInterval;

            scanPosition += speedFactor * grainSizeSamples;
            if (scanPosition >= sampleLength - grainSizeSamples) {
                scanPosition = 0.0f;
            }

            int jitter = (rand() % (grainSizeSamples / 3)) - (grainSizeSamples / 6);
            int pos = static_cast<int>(scanPosition + jitter);
            pos = std::max(0, std::min(pos, sampleLength - grainSizeSamples - 1));
            granular->triggerGrain(pos);
        }

        float out = granular->processGrains();
        out *= 0.38f;

        static float lfoPhase = 0.0f;
        lfoPhase += 0.25f / sr;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float lfo = sinf(lfoPhase * 2.0f * M_PI) * 12.0f;

        float delayTimeMs = 500.0f + touchX * 600.0f + lfo;
        delay->setTime(delayTimeMs);
        delay->setFeedback(0.3f + touchY * 0.6f);
        delay->setDiffusion(0.15f + touchSize * 0.5f);

        out = delay->process(out);

        for (unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
            audioWrite(context, n, ch, out);
        }
    }
}

void cleanup(BelaContext* context, void* userData)
{
    delete granular;
    delete delay;
}