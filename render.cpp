#include <Bela.h>
#include <libraries/Trill/Trill.h>
#include <libraries/Gui/Gui.h>
#include <libraries/GuiController/GuiController.h>
#include <unistd.h>
#include <cmath>
#include <cstdlib>

#include "GranularSynth.h"
#include "LluonDelay.h"

// Global objects
GranularSynth* granular = nullptr;
LluonDelay* delay = nullptr;
Trill trill;
Gui gui;
GuiController controller;

// Slider indices (returned by addSlider)
int scanSpeedIdx = -1;
int grainDensityIdx = -1;
int delayFeedbackIdx = -1;

// GUI parameters (default values – will be updated by sliders)
float scanSpeed = 0.4f;
float grainDensity = 0.5f;
float delayFeedback = 0.5f;

// Trill values
float touchX = 0.5f;
float touchY = 0.5f;
float touchSize = 0.0f;
bool trillDetected = false;

// Granular control
float scanPosition = 0.0f;
float grainTimer = 0.0f;
const float baseGrainInterval = 0.09f;

void readTrillLoop(void*)
{
    while (!Bela_stopRequested()) {
        if (trillDetected) {
            trill.readI2C();
            float x = trill.compoundTouchHorizontalLocation();
            float y = trill.compoundTouchLocation();
            float size = trill.compoundTouchSize();
            if (x >= 0.0f) touchX = x;
            if (y >= 0.0f) touchY = y;
            if (size >= 0.0f) touchSize = size;
        }
        usleep(5000);
    }
}

bool setup(BelaContext* context, void* userData)
{
    srand(123);

    granular = new GranularSynth(context->audioSampleRate, "Ornella.wav", 140.0f, 1.0f, 50);
    if (granular->getAudioSampleLength() == 0) {
        rt_printf("Failed to load Ornella.wav\n");
        return false;
    }

    delay = new LluonDelay(static_cast<int>(context->audioSampleRate),
                           3 * static_cast<int>(context->audioSampleRate));

    // GUI setup
    gui.setup(context->projectName);
    controller.setup(&gui, "Controls");  // Attach controller to gui with a folder name

    // Add sliders and store their indices
    scanSpeedIdx     = controller.addSlider("Scan Speed",     0.4f, 0.0f, 1.0f, 0.01f);
    grainDensityIdx  = controller.addSlider("Grain Density",  0.5f, 0.0f, 1.0f, 0.01f);
    delayFeedbackIdx = controller.addSlider("Delay Feedback", 0.5f, 0.0f, 1.0f, 0.01f);

    // Trill Flex (optional)
    if (trill.setup(1, Trill::FLEX, 0x28) == 0) {
        trillDetected = true;
        trill.setMode(Trill::CENTROID);
        trill.setNoiseThreshold(0.06f);
        trill.setScanSettings(3, 12);
    } else {
        trillDetected = false;
    }

    AuxiliaryTask trillTask = Bela_createAuxiliaryTask(readTrillLoop, 90, "trill-reader");
    Bela_scheduleAuxiliaryTask(trillTask);

    return true;
}

void render(BelaContext* context, void* userData)
{
    // Update parameters from GUI sliders
    scanSpeed     = controller.getSliderValue(scanSpeedIdx);
    grainDensity  = controller.getSliderValue(grainDensityIdx);
    delayFeedback = controller.getSliderValue(delayFeedbackIdx);

    int sampleLength = granular->getAudioSampleLength();
    int grainSizeSamples = granular->getGrainSizeSamples();
    float sr = context->audioSampleRate;

    for (unsigned int n = 0; n < context->audioFrames; n++) {
        bool touched = trillDetected && (touchSize > 0.02f);

        // Scan speed
        float baseSpeed = 0.1f + scanSpeed * 0.6f;
        float trillSpeed = touched ? (0.1f + touchX * 0.6f) : baseSpeed;
        float speedFactor = trillDetected ? (trillSpeed * 0.5f + baseSpeed * 0.5f) : baseSpeed;

        // Grain density
        float baseDensity = 0.3f + grainDensity * 5.7f;
        float trillDensity = touched ? (1.0f + touchSize * 5.0f) : baseDensity;
        float density = trillDetected ? (trillDensity * 0.5f + baseDensity * 0.5f) : baseDensity;

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

        // LFO for delay time variation
        static float lfoPhase = 0.0f;
        lfoPhase += 0.25f / sr;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float lfo = sinf(lfoPhase * 2.0f * M_PI) * 12.0f;

        float delayTimeMs = 500.0f + (trillDetected ? touchX : scanSpeed) * 600.0f + lfo;

        // Delay feedback
        float baseFeedback = 0.3f + delayFeedback * 0.6f;
        float trillFeedback = touched ? (0.3f + touchY * 0.6f) : baseFeedback;
        float feedback = trillDetected ? (trillFeedback * 0.5f + baseFeedback * 0.5f) : baseFeedback;

        delay->setTime(delayTimeMs);
        delay->setFeedback(feedback);
        delay->setDiffusion(0.15f + (trillDetected ? touchSize : grainDensity) * 0.5f);

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