#pragma once

#include "audioKernel.h"

#include <miniaudio/miniaudio.h>
#include <vector>
#include <mutex>

class AudioEngine
{
public:
	AudioEngine();
	~AudioEngine();

	bool Initialize();
	void Deinitialize();
	void Update();

	static void audioDeviceDataCallback(ma_device* device, void* output, const void* input, ma_uint32 frameCount);

	ma_decoder m_audioDecoder;
	ma_device m_audioDevice;
	const char* m_songFilePath = "music/turiip.mp3";

	std::vector<float> m_pcmRingBuffer;
	std::mutex m_audioMutex;
	std::vector<float> m_localPCMFrame;

	SongFeatures* m_dev_songFeatures;
};

