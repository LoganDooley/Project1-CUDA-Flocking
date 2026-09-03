#include "audioEngine.h"

#include <iostream>

AudioEngine::AudioEngine() :
	m_pcmRingBuffer(AUDIO_FFT_SIZE, 0.0f),
	m_localPCMFrame(AUDIO_FFT_SIZE, 0.0f)
{

}

AudioEngine::~AudioEngine()
{
    ma_device_uninit(&m_audioDevice);
    ma_decoder_uninit(&m_audioDecoder);
}

bool AudioEngine::Initialize() {
    ma_result result = ma_decoder_init_file(m_songFilePath, NULL, &m_audioDecoder);
    if (result != MA_SUCCESS) {
        std::cout << "Error: Failed to load" << std::string(m_songFilePath) << "\n";;
        return false;
    }

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = m_audioDecoder.outputFormat;
    deviceConfig.playback.channels = m_audioDecoder.outputChannels;
    deviceConfig.sampleRate = m_audioDecoder.outputSampleRate;
    deviceConfig.dataCallback = audioDeviceDataCallback;
    deviceConfig.pUserData = this;

    result = ma_device_init(NULL, &deviceConfig, &m_audioDevice);
    if (result != MA_SUCCESS) {
        std::cout << "Error: Failed to init miniaudio device.\n";
        ma_decoder_uninit(&m_audioDecoder);
        return -1;
    }

    ma_device_start(&m_audioDevice);

    Audio::initAudioFFT(&m_dev_songFeatures);
}

void AudioEngine::Deinitialize() {
    Audio::endAudioFFT(m_dev_songFeatures);
}

void AudioEngine::Update()
{
    {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        std::copy(m_pcmRingBuffer.begin(), m_pcmRingBuffer.end(), m_localPCMFrame.begin());
    }

    Audio::processPCM(m_localPCMFrame.data(), m_dev_songFeatures);
}

void AudioEngine::audioDeviceDataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount)
{
    AudioEngine* audioEngine = (AudioEngine*)device->pUserData;
    if (audioEngine == nullptr) {
        return;
    }

    if (frameCount == 0) {
        return;
    }

    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(&audioEngine->m_audioDecoder, output, frameCount, &framesRead);

    float* samples = (float*)output;
    ma_uint32 channels = device->playback.channels;

    // In a callback so need to lock
    std::lock_guard<std::mutex> lock(audioEngine->m_audioMutex);

    for (ma_uint32 i = 0; i < framesRead; i++) {
        float monoSample = 0.0f;
        for (ma_uint32 c = 0; c < channels; c++) {
            monoSample += samples[i * channels + c];
        }
        monoSample /= (float)channels;
        audioEngine->m_pcmRingBuffer.push_back(monoSample);
    }

    // Scale down to most recent samples within fft size
    if (audioEngine->m_pcmRingBuffer.size() > AUDIO_FFT_SIZE) {
        audioEngine->m_pcmRingBuffer.erase(audioEngine->m_pcmRingBuffer.begin(), audioEngine->m_pcmRingBuffer.end() - AUDIO_FFT_SIZE);
    }
}
