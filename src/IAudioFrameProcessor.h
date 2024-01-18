#pragma once
#include <stdint.h>
#include <vector>
#include "audio_process.h"

struct AudioFrame
{
	uint32_t nSamples;
	uint32_t nChannel = 1;
	float* samples = nullptr;
	AudioFrame();
	AudioFrame(AudioFrame&);
	AudioFrame(const AudioFrame&);
	AudioFrame(AudioFrame&&);
	AudioFrame& operator=(const AudioFrame&);
	void Allocate(uint32_t sample_count, float* data = nullptr, int channel_count = 1);
	void Release();
};

struct AudioFormat
{
	uint32_t sampleRate;
	uint32_t frame_size;
	uint32_t nChannels;
};
enum class AudioProcessType
{
	Process = 0,
	IO = 1
};
class IAudioFrameProcessor
{
protected:
	AudioFormat input_format;
	AudioFormat output_format;
public:
	virtual float GetEstimateDelay() { return 0; }//ms
	virtual bool SetInputFormat(AudioFormat*) { return false; }
	virtual bool SetOutputFormat(AudioFormat*) { return false; }
	virtual bool GetInputFormat(AudioFormat*) { return false; }
	virtual bool GetOutputFormat(AudioFormat*) { return false; }
	virtual bool Input(AudioFrame& f) { return false; }//return value ignored
	virtual uint32_t Output(AudioFrame* f) { return false; }
	virtual void Process(AudioFrame& f) {};

	virtual AudioProcessType Type() { return AudioProcessType::IO; };
};
class FrameAligner : public IAudioFrameProcessor
{
	uint32_t target_size;
	uint32_t nBuffered;
	uint32_t nChannel;
	float* buffer;
	std::vector<AudioFrame> frames;
public:
	FrameAligner();
	FrameAligner(uint32_t target_frame_size, uint32_t channel_count = 1);
	FrameAligner(const FrameAligner& f);
	FrameAligner(FrameAligner&& f) noexcept;
	FrameAligner& operator =(const FrameAligner& f);
	~FrameAligner();
	bool Input(AudioFrame& f);
	uint32_t Output(AudioFrame* f);
	uint32_t GetFrameSize();

};

class Resampler : public IAudioFrameProcessor
{
	void* ctx = nullptr;
	void* dat = nullptr;
	int buffer_len = 0;
	float* buffer = nullptr;
	int output_len = 0;
	int unused_len = 0;
	int unused_buffer_len = 0;
	float* unused_buffer = nullptr;
public:
	enum class QUALITY
	{
		SINC_BEST = 0,
		SINC_MEDIUM = 1,
		SINC_FASTEST = 2,
		NO_HOLD = 3,
		LINEAR = 4,
	};
	Resampler();
	Resampler(uint32_t i_sr, uint32_t o_sr, QUALITY quality = QUALITY::SINC_BEST);
	~Resampler();
	bool Input(AudioFrame& f);
	uint32_t Output(AudioFrame* f);
	float GetEstimateDelay();
};
class SincResample : public IAudioFrameProcessor
{
	uint32_t sr_i;
	uint32_t sr_o;
	uint32_t sz_filter;
	float ratio;
	uint32_t nOut = false;
	uint32_t nRight = 0;
	uint32_t hf = 0;

	float* sLeft = nullptr;
	float* sRight = nullptr;
	std::vector<float> out;

public:
	SincResample(uint32_t i_sr, uint32_t o_sr, uint32_t filter_size = 80);
	~SincResample();
	bool Input(AudioFrame& f);
	uint32_t Output(AudioFrame* f);
	float GetEstimateDelay();

};
class RNNDenoise : public IAudioFrameProcessor
{
	void* ctx = nullptr;
public:
	RNNDenoise();
	void Process(AudioFrame& f);
	float GetEstimateDelay() { return 5; };
	AudioProcessType Type() { return AudioProcessType::Process; };
};

class AudioCapture : public IAudioFrameProcessor
{

public:

	uint32_t Output(AudioFrame* f);
};

class ChannelUtil
{
public:
	static bool Combine(AudioFrame& out, AudioFrame* in, int nChannel);
	static int Split(AudioFrame& in, AudioFrame* out);
	static void Clamp(AudioFrame& in, float max_db);
	static void ClampMul(AudioFrame& in, float max_db);
	static void Gain(AudioFrame& in, float db);
	static void GainMul(AudioFrame& in, float mul);

};

class Compressor : public IAudioFrameProcessor
{
	int samplerate = 48000;
	float threshold = -18.f;
	float ratio = 10.f;
	float gain = 1.f;
	float attack_gain;
	float release_gain;
	float attack_ms = 6;
	float release_ms = 60;
	float slope = 0.9f;

	float envelope = 0;
	float gain_coefficient(uint32_t sample_rate, float time)
	{
		return (float)exp(-1.0f / (sample_rate * time));
	}
public:

	Compressor(int samplerate = 48000);
	float Ratio(float r);
	float Ratio();
	float Threshold(float t);
	float Threshold();
	float Gain(float g);
	float Gain();
	float AttackTime(float ms);
	float AttackTime();
	float ReleaseTime(float ms);
	float ReleaseTime();
	void Process(AudioFrame& f);
	AudioProcessType Type() { return AudioProcessType::Process; };

};
class GainClamp : public IAudioFrameProcessor
{
	float gain = 1;
	float clamp = 1;
public:
	GainClamp(float gain = 0, float clamp = 0);
	float Gain();
	float Gain(float gain);
	float Clamp();
	float Clamp(float clamp);
	void Process(AudioFrame& f);
	AudioProcessType Type() { return AudioProcessType::Process; };
};