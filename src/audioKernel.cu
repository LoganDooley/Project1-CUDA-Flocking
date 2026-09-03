#include "audioKernel.h"

#include <cuda.h>
#include <cufft.h>

#define bassMultiplier 100.f
#define midMultiplier 1000.f
#define trebleMultiplier 10000.f

// Audio device pointers
float* dev_pcmData = nullptr;
cufftComplex* dev_fftResult = nullptr;
float* dev_frequencies = nullptr;
SongFeatures* dev_songFeatures = nullptr;
cufftHandle fftHandle;

void Audio::initAudioFFT(SongFeatures** dev_songFeatures) {
    cudaMalloc((void**)&dev_pcmData, AUDIO_FFT_SIZE * sizeof(float));

    cudaMalloc((void**)&dev_fftResult, (AUDIO_FFT_SIZE / 2 + 1) * sizeof(cufftComplex));

    cudaMalloc((void**)&dev_frequencies, FREQ_BUCKETS * sizeof(float));

    cudaMalloc((void**)dev_songFeatures, sizeof(SongFeatures));

    cufftPlan1d(&fftHandle, AUDIO_FFT_SIZE, CUFFT_R2C, 1);
}

__global__ void kernComputeFreqMagnitudes(cufftComplex* fftData, float* magnitudes, int numBuckets) {
    int index = blockIdx.x * blockDim.x + threadIdx.x;
    if (index > numBuckets) {
        return;
    }

    cufftComplex sample = fftData[index];
    float magnitude = sqrtf(sample.x * sample.x + sample.y * sample.y);

    float normalizedMagnitude = (magnitude / (float)AUDIO_FFT_SIZE) * 2.0f;
    magnitudes[index] = normalizedMagnitude;
}

__global__ void kernUpdateSongFeatures(float* magnitudes, SongFeatures* songFeatures, int numBuckets) {
    // Just launching a single thread (not super efficient I know)
    if (threadIdx.x != 0 || blockIdx.x != 0) {
        return;
    }

    int bassEnd = 16;
    int midEnd = 150;

    float sumBass = 0.0f;
    for (int i = 0; i < bassEnd; i++) {
        sumBass += magnitudes[i];
    }

    float sumMid = 0.0f;
    for (int i = bassEnd; i < midEnd; i++) {
        sumMid += magnitudes[i];
    }

    float sumTreble = 0.0f;
    for (int i = midEnd; i < numBuckets; i++) {
        sumTreble += magnitudes[i];
    }

    songFeatures->bass = bassMultiplier * sumBass / (float)bassEnd;
    songFeatures->mid = midMultiplier * sumMid / (float)(midEnd - bassEnd);
    songFeatures->treble = trebleMultiplier * sumTreble / (float)(numBuckets - midEnd);
}

void Audio::processPCM(const float* host_pcmData, SongFeatures* dev_songFeatures)
{
    cudaMemcpyAsync(dev_pcmData, host_pcmData, AUDIO_FFT_SIZE * sizeof(float), cudaMemcpyHostToDevice);
    // Run FFT
    cufftExecR2C(fftHandle, dev_pcmData, dev_fftResult);

    int fftBlockSize = 256;
    int fftGridSize = (FREQ_BUCKETS + fftBlockSize - 1) / fftBlockSize;

    kernComputeFreqMagnitudes << <fftGridSize, fftBlockSize >> > (dev_fftResult, dev_frequencies, FREQ_BUCKETS);

    kernUpdateSongFeatures << <1, 1 >> > (dev_frequencies, dev_songFeatures, FREQ_BUCKETS);

    // Make sure this is done so we can read the frequencies in the boid sim
    cudaDeviceSynchronize();
}

void Audio::endAudioFFT(SongFeatures* dev_songFeatures) {
    cufftDestroy(fftHandle);
    cudaFree(dev_pcmData);
    cudaFree(dev_fftResult);
    cudaFree(dev_frequencies);
    cudaFree(dev_songFeatures);
}