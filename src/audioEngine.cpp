#include "audioEngine.h"

#include "imgui.h"

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

    if (audioEngine->m_seekRequested.load()) {
        ma_uint64 length = 0;
        if (ma_decoder_get_length_in_pcm_frames(&audioEngine->m_audioDecoder, &length) == MA_SUCCESS) {
            ma_uint64 targetFrame = (ma_uint64)(audioEngine->m_targetProgress.load() * (float)length);
            ma_decoder_seek_to_pcm_frame(&audioEngine->m_audioDecoder, targetFrame);
        }
        audioEngine->m_seekRequested.store(false);
    }

    ma_uint64 framesRead = 0;
    ma_decoder_read_pcm_frames(&audioEngine->m_audioDecoder, output, frameCount, &framesRead);

    if (framesRead == 0 && audioEngine->m_loop) {
        ma_decoder_seek_to_pcm_frame(&audioEngine->m_audioDecoder, 0);
        ma_decoder_read_pcm_frames(&audioEngine->m_audioDecoder, output, frameCount, &framesRead);
    }

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

void AudioEngine::ToggleIsPlaying()
{
    m_isPlaying = !m_isPlaying;
    if (m_isPlaying) {
        ma_device_start(&m_audioDevice);
    }
    else {
        ma_device_stop(&m_audioDevice);
    }
}

float AudioEngine::GetCurrentSongProgress()
{
    ma_uint64 cursor = 0;
    if (ma_decoder_get_cursor_in_pcm_frames(&m_audioDecoder, &cursor) != MA_SUCCESS) {
        return 0.0f;
    }

    ma_uint64 length = 0;
    if (ma_decoder_get_length_in_pcm_frames(&m_audioDecoder, &length) != MA_SUCCESS) {
        return 0.0f;
    }

    return (float)cursor / (float)length;
}

void AudioEngine::SetCurrentSongProgress(float progress)
{
    m_targetProgress.store(progress);
    m_seekRequested.store(true);
}

void AudioEngine::RenderAudioPlayer()
{
    ImGui::Begin("Audio Player", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Playing: %s", m_songFilePath);
    ImGui::Separator();

    float currentSongProgress = GetCurrentSongProgress();
    float targetSongProgress = currentSongProgress;
    if (ImGui::SliderFloat("Progress", &targetSongProgress, 0.0f, 1.0f, "")) {
        SetCurrentSongProgress(targetSongProgress);
    }

    ImGui::Text("Playback Progress: %.1f%%", currentSongProgress * 100.f);
    ImGui::Spacing();

    if (m_isPlaying) {
        if (ImGui::Button("Pause")) {
            ToggleIsPlaying();
        }
    }
    else {
        if (ImGui::Button("Play")) {
            ToggleIsPlaying();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Restart")) {
        SetCurrentSongProgress(0.0f);
    }

    ImGui::SameLine();

    ImGui::Checkbox("Loop", &m_loop);

    ImGui::Separator();

    if (ImGui::Button("Open Audio File...")) {
        // TODO: Maybe import tinyfiledialogs 
        // and load mp3s at runtime 
    }

    ImGui::End();
}
