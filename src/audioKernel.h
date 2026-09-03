#pragma once

// Audio constants
constexpr size_t AUDIO_FFT_SIZE = 1024;
constexpr size_t FREQ_BUCKETS = AUDIO_FFT_SIZE / 2;

struct SongFeatures {
    float bass;
    float mid;
    float treble;
};

namespace Audio {
    void initAudioFFT(SongFeatures** dev_songFeatures);
    void processPCM(const float* host_pcmData, SongFeatures* dev_songFeatures);
    void endAudioFFT(SongFeatures* dev_songFeatures);
}