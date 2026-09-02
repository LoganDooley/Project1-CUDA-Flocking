#pragma once

// Audio constants
constexpr size_t AUDIO_FFT_SIZE = 1024;
constexpr size_t FREQ_BUCKETS = AUDIO_FFT_SIZE / 2;

namespace Boids {
    void initSimulation(int N);
    void stepSimulationNaive(float dt);
    void stepSimulationScatteredGrid(float dt);
    void stepSimulationCoherentGrid(float dt);
    void stepSimulationCoherentGridWithAudio(float dt);
    void copyBoidsToVBO(float *vbodptr_positions, float *vbodptr_velocities);

    void endSimulation();
    void unitTest();
}

namespace Audio {
    void initAudioFFT();
    void processPCM(const float* host_pcmData);
    void endAudioFFT();
}
