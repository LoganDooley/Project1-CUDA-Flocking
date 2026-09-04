#include "audioEngine.h"

#include "imgui.h"
#include "nfd.hpp"

#include <iostream>

AudioEngine::AudioEngine() :
    m_songFilePath(""),
	m_pcmRingBuffer(AUDIO_FFT_SIZE, 0.0f),
	m_localPCMFrame(AUDIO_FFT_SIZE, 0.0f),
    m_isPlaying(false)
{

}

AudioEngine::~AudioEngine()
{
    
}

bool AudioEngine::Initialize() {
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = 48000;
    deviceConfig.dataCallback = audioDeviceDataCallback;
    deviceConfig.pUserData = this;

    ma_result result = ma_device_init(NULL, &deviceConfig, &m_audioDevice);
    if (result != MA_SUCCESS) {
        std::cout << "Error: Failed to init miniaudio device.\n";
        ma_decoder_uninit(&m_audioDecoder);
        return false;
    }

    Audio::initAudioFFT(&m_dev_songFeatures);

    return true;
}

void AudioEngine::Deinitialize() {
    ma_device_uninit(&m_audioDevice);
    if (!m_songFilePath.empty()) {
        ma_decoder_uninit(&m_audioDecoder);
        m_songFilePath = "";
    }

    Audio::endAudioFFT(m_dev_songFeatures);
}

void AudioEngine::Update()
{
    if (m_songFilePath.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_audioMutex);
        std::copy(m_pcmRingBuffer.begin(), m_pcmRingBuffer.end(), m_localPCMFrame.begin());
    }

    Audio::processPCM(m_localPCMFrame.data(), m_dev_songFeatures);
}

void AudioEngine::audioDeviceDataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount)
{
    AudioEngine* audioEngine = (AudioEngine*)device->pUserData;
    if (audioEngine == nullptr || audioEngine->m_songFilePath.empty()) {
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

    float currentVolume = 1.0f;
    ma_device_get_master_volume(device, &currentVolume);

    // In a callback so need to lock
    std::lock_guard<std::mutex> lock(audioEngine->m_audioMutex);

    for (ma_uint32 i = 0; i < framesRead; i++) {
        float monoSample = 0.0f;
        for (ma_uint32 c = 0; c < channels; c++) {
            monoSample += samples[i * channels + c];
        }
        monoSample /= (float)channels;
        // scale pcm samples with miniaudio master volume
        monoSample *= currentVolume;
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

    if (m_songFilePath.empty()) {
        // Red-ish color
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No Track Loaded");
        ImGui::Separator();

        // Show slider and play button as disabled
        ImGui::BeginDisabled();
        float dummyProgress = 0.0f;
        ImGui::SliderFloat("Progress", &dummyProgress, 0.0f, 1.0f, "");
        ImGui::Button("Play");
        ImGui::EndDisabled();
    }
    else {
        ImGui::Text("Playing: %s", m_songFilePath.c_str());
        ImGui::Separator();

        float currentSongProgress = GetCurrentSongProgress();
        float targetSongProgress = currentSongProgress;
        if (ImGui::SliderFloat("Progress", &targetSongProgress, 0.0f, 1.0f, "")) {
            SetCurrentSongProgress(targetSongProgress);
        }
        ImGui::Text("Playback Progress: %.1f%%", currentSongProgress * 100.f);
        
        ImGui::Spacing();

        // Volume slider
        float currentVolume = 0.0f;
        ma_device_get_master_volume(&m_audioDevice, &currentVolume);

        float volumePercent = currentVolume * 100.f;
        if (ImGui::SliderFloat("Volume", &volumePercent, 0.0f, 100.0f, "%.0f%%")) {
            ma_device_set_master_volume(&m_audioDevice, volumePercent / 100.f);
        }

        ImGui::Spacing();

        if (m_isPlaying) {
            if (ImGui::Button("Pause")) { ToggleIsPlaying(); }
        }
        else {
            if (ImGui::Button("Play")) { ToggleIsPlaying(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart")) { SetCurrentSongProgress(0.0f); }
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &m_loop);
    }

    ImGui::Separator();
    if (ImGui::Button("Open Audio File...")) {
        PickAudioFile();
    }
    ImGui::End();
}

void AudioEngine::PickAudioFile()
{
    NFD::Guard nfdGuard;
    nfdfilteritem_t filterItem[1] = {
        { "Audio Files", 
        "mp3,wav,ogg,flac" } 
    };
    NFD::UniquePath outPath;

    nfdresult_t result = NFD::OpenDialog(outPath, filterItem, 1, "audio");

    if (result != NFD_OKAY) {
        return;
    }

    if (!m_songFilePath.empty()) {
        ma_device_stop(&m_audioDevice);
        ma_decoder_uninit(&m_audioDecoder);
    }

    m_songFilePath = outPath.get();

    ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 48000);

    ma_result initResult = ma_decoder_init_file(m_songFilePath.c_str(), &decoderConfig, &m_audioDecoder);
    if (initResult != MA_SUCCESS) {
        std::cout << "Error: Failed to load audio file: " << m_songFilePath << "\n";
        m_songFilePath = "";
        return;
    }

    // Start playing immediately
    m_isPlaying = true;
    ma_device_start(&m_audioDevice);
    SetCurrentSongProgress(0.0f);
}
