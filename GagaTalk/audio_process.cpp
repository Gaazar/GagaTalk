#include "math.h"
#include "IAudioFrameProcessor.h"
#include <memory.h>
#include <rnnoise.h>
#include <assert.h>
#pragma comment(lib,"librnnoise.lib")


AudioFrame::AudioFrame()
{

}
AudioFrame::AudioFrame(AudioFrame&& f)
{
	nSamples = f.nSamples;
	samples = f.samples;
}

AudioFrame::AudioFrame(AudioFrame& f)
{
	nSamples = f.nSamples;
	Allocate(nSamples, f.samples);
}
AudioFrame& AudioFrame::operator=(const AudioFrame& f)
{
	nSamples = f.nSamples;
	samples = f.samples;
	return *this;
}

void AudioFrame::Allocate(uint32_t sample_count, float* data, int channel_count)
{
	if (samples)
	{
		if (sample_count * channel_count != nSamples * nChannel)
		{
			delete[] samples;
			samples = new float[sample_count * channel_count];
		}
	}
	else
	{
		samples = new float[sample_count * channel_count];
	}
	nSamples = sample_count;
	nChannel = channel_count;
	if (data)
		memcpy(samples, data, sizeof(float) * sample_count * channel_count);
}
void AudioFrame::Release()
{
	if (samples)
		delete[] samples;
	samples = nullptr;
}


RNNDenoise::RNNDenoise()
{
	ctx = rnnoise_create(NULL);
	//buffer = new float[480];
}
void RNNDenoise::Process(AudioFrame& f)
{
	assert(f.nSamples == 480);
	for (int i = 0; i < f.nSamples; i++)
		f.samples[i] *= 32767.f;
	rnnoise_process_frame((DenoiseState*)ctx, f.samples, f.samples);
	for (int i = 0; i < f.nSamples; i++)
		f.samples[i] /= 32767.f;
}

bool ChannelUtil::Combine(AudioFrame& out, AudioFrame* in, int nChannel)
{
	int n_sample = in->nSamples;
	out.Allocate(n_sample, nullptr, nChannel);
	for (int i = 0; i < nChannel; i++)
	{
		assert(in[nChannel].nChannel == 1 && in[nChannel].nSamples == n_sample);
		for (int n = 0; n < n_sample; n++)
		{
			out.samples[n * nChannel + i] = in[i].samples[n];
		}

	}
	return true;
}
int ChannelUtil::Split(AudioFrame& in, AudioFrame* out) //return channel count
{
	int n_channel = in.nChannel;
	int n_sample = in.nSamples;
	for (int i = 0; i < n_channel; i++)
	{
		out[i].Allocate(n_sample);
		for (int n = 0; n < n_sample; n++)
		{
			out[i].samples[n] = in.samples[n * n_channel + i];
		}

	}
	return n_channel;
}

void ChannelUtil::Clamp(AudioFrame& in, float max_db)
{
	float mul = db_to_mul(max_db);
	for (int i = 0; i < in.nSamples; i++)
	{
		for (int n = 0; n < in.nChannel; n++)
		{
			if (in.samples[i * in.nChannel + n] > mul)
			{
				in.samples[i * in.nChannel + n] = mul;
			}
			else if (in.samples[i * in.nChannel + n] < -mul)
				in.samples[i * in.nChannel + n] = -mul;

		}
	}
}
void ChannelUtil::ClampMul(AudioFrame& in, float mul)
{
	for (int i = 0; i < in.nSamples; i++)
	{
		for (int n = 0; n < in.nChannel; n++)
		{
			if (in.samples[i * in.nChannel + n] > mul)
			{
				in.samples[i * in.nChannel + n] = mul;
			}
			else if (in.samples[i * in.nChannel + n] < -mul)
				in.samples[i * in.nChannel + n] = -mul;

		}
	}
}

void ChannelUtil::Gain(AudioFrame& in, float db)
{
	float mul = db_to_mul(db);
	for (int i = 0; i < in.nSamples; i++)
	{
		for (int n = 0; n < in.nChannel; n++)
		{
			in.samples[i * in.nChannel + n] *= mul;
		}
	}
}
void ChannelUtil::GainMul(AudioFrame& in, float mul)
{
	for (int i = 0; i < in.nSamples; i++)
	{
		for (int n = 0; n < in.nChannel; n++)
		{
			in.samples[i * in.nChannel + n] *= mul;
		}
	}
}

GainClamp::GainClamp(float gain, float clamp)
{
	this->gain = db_to_mul(gain);
	this->clamp = db_to_mul(clamp);
}
float GainClamp::Gain()
{
	return  mul_to_db(gain);
}
float GainClamp::Gain(float gain)
{
	this->gain = db_to_mul(gain);
	return gain;
}
float GainClamp::Clamp()
{
	return  mul_to_db(clamp);
}
float GainClamp::Clamp(float clamp)
{
	this->clamp = db_to_mul(clamp);
	return clamp;
}
void GainClamp::Process(AudioFrame& f)
{
	ChannelUtil::GainMul(f, gain);
	ChannelUtil::ClampMul(f, clamp);
}