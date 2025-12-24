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

// Slider indices
int scanSpeedIdx = -1;
int grainDensityIdx = -1;
int delayFeedbackIdx = -1;

// Base GUI parameters
float baseScanSpeed = 0.4f;
float baseGrainDensity = 0.5f;
float baseDelayFeedback = 0.5f;

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
    controller.setup(&gui, "Controls");

    scanSpeedIdx     = controller.addSlider("Scan Speed",     0.4f, 0.0f, 1.0f, 0.01f);
    grainDensityIdx  = controller.addSlider("Grain Density",  0.5f, 0.0f, 1.0f, 0.01f);
    delayFeedbackIdx = controller.addSlider("Delay Feedback", 0.5f, 0.0f, 1.0f, 0.01f);

    // Trill Square setup - CENTROID mode for reliable single-touch X/Y + size
    if (trill.setup(1, Trill::SQUARE, 0x28) == 0) {
        trillDetected = true;
        trill.setMode(Trill::CENTROID);  // Best for accurate X/Y position + size on Square
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
    // Update base parameters from GUI
    baseScanSpeed     = controller.getSliderValue(scanSpeedIdx);
    baseGrainDensity  = controller.getSliderValue(grainDensityIdx);
    baseDelayFeedback = controller.getSliderValue(delayFeedbackIdx);

    int sampleLength = granular->getAudioSampleLength();
    int grainSizeSamples = granular->getGrainSizeSamples();
    float sr = context->audioSampleRate;

    for (unsigned int n = 0; n < context->audioFrames; n++) {
        float touchX = 0.5f;
        float touchY = 0.5f;
        float touchSize = 0.0f;
        bool touched = false;

        if (trillDetected) {
            float x = trill.compoundTouchHorizontalLocation();  // X position (0-1 left to right)
            float y = trill.compoundTouchLocation();           // Y position (0-1 bottom to top)
            float size = trill.compoundTouchSize();            // Pressure-like size

            if (size > 0.02f) {
                touchX = x >= 0 ? x : 0.5f;
                touchY = y >= 0 ? y : 0.5f;
                touchSize = size;
                touched = true;
            }
        }

        // === 5 expressive parameters from single touch on Trill Square ===
        // 1. -X (left side): reverse scan speed
        float leftInfluence = (1.0f - touchX) * touchSize;   // Stronger when left + pressed

        // 2. +X (right side): forward scan speed + longer delay time
        float rightInfluence = touchX * touchSize;          // Stronger when right + pressed

        // 3. -Y (bottom): higher grain density
        float bottomInfluence = (1.0f - touchY) * touchSize;

        // 4. +Y (top): higher delay feedback
        float topInfluence = touchY * touchSize;

        // 5. Pressure (touchSize): higher diffusion (washier delay)
        float pressure = touchSize;

        // Scan speed/direction
        float speedFactor = 0.1f + baseScanSpeed * 0.6f + (rightInfluence - leftInfluence) * 0.8f;

        // Grain density
        float density = 0.3f + baseGrainDensity * 5.7f + bottomInfluence * 4.0f;

        // Delay time (longer toward right)
        float delayTimeMs = 300.0f + rightInfluence * 800.0f;

        // Delay feedback (more toward top)
        float feedback = 0.3f + baseDelayFeedback * 0.6f + topInfluence * 0.6f;

        // Diffusion (stronger with pressure)
        float diffusion = 0.15f + pressure * 0.65f;

        // LFO for subtle variation
        static float lfoPhase = 0.0f;
        lfoPhase += 0.25f / sr;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float lfo = sinf(lfoPhase * 2.0f * M_PI) * 12.0f;
        delayTimeMs += lfo;

        // Granular triggering
        float currentInterval = baseGrainInterval / density;
        grainTimer += 1.0f / sr;
        if (grainTimer >= currentInterval) {
            grainTimer -= currentInterval;

            scanPosition += speedFactor * grainSizeSamples;
            while (scanPosition >= sampleLength - grainSizeSamples) scanPosition -= (sampleLength - grainSizeSamples);
            while (scanPosition < 0) scanPosition += (sampleLength - grainSizeSamples);

            int jitter = (rand() % (grainSizeSamples / 3)) - (grainSizeSamples / 6);
            int pos = static_cast<int>(scanPosition + jitter);
            pos = std::max(0, std::min(pos, sampleLength - grainSizeSamples - 1));

            granular->triggerGrain(pos);
        }

        float out = granular->processGrains();
        out *= 0.38f;

        delay->setTime(delayTimeMs);
        delay->setFeedback(feedback);
        delay->setDiffusion(diffusion);

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