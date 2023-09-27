#include "IAudioFrameProcessor.h"
#include <assert.h>
#include <samplerate.h>
#pragma comment(lib,"samplerate.lib")

Resampler::Resampler(uint32_t i_sr, uint32_t o_sr, QUALITY quality)
{
	int err = 0;
	input_format.sampleRate = i_sr;
	output_format.sampleRate = o_sr;
	buffer_len = o_sr / 100;
	buffer = new float[buffer_len] {0};

	ctx = src_new((int)quality, 1, &err);
	dat = new SRC_DATA;
	SRC_DATA& d = *(SRC_DATA*)dat;
	d.end_of_input = 0;
	d.src_ratio = (float)o_sr / (float)i_sr;
	d.data_out = buffer;
	d.input_frames = 0;
	d.output_frames = buffer_len;
}
Resampler::Resampler()
{

}
Resampler::~Resampler()
{
	if (ctx)
		src_delete((SRC_STATE*)ctx);
	if (dat)
		delete dat;
	if (buffer)
		delete[] buffer;
	buffer_len = 0;
	ctx = nullptr;
	dat = nullptr;
	buffer = nullptr;
}

bool Resampler::Input(AudioFrame& f)
{
	float ratio = (float)input_format.sampleRate / output_format.sampleRate;
	SRC_DATA& d = *(SRC_DATA*)dat;
	SRC_STATE& c = *(SRC_STATE*)ctx;
	int e = 0;
	if (f.nSamples * d.src_ratio > buffer_len)
	{
		delete[] buffer;
		buffer_len = ceilf(f.nSamples * d.src_ratio);
		buffer = new float[buffer_len] {0};
		d.output_frames = buffer_len;
		d.data_out = buffer;
	}
	if (unused_len == 0)
	{
		d.data_in = f.samples;
	}
	else
	{
		if (unused_len + f.nSamples > unused_buffer_len)
		{
			unused_buffer_len = unused_len + f.nSamples;
			float* nbuf = new float[unused_buffer_len];
			memcpy(nbuf, unused_buffer, unused_len * sizeof(float));
			if (unused_buffer)
				delete unused_buffer;
			unused_buffer = nbuf;
		}
		memcpy(unused_buffer + unused_len, f.samples, f.nSamples * sizeof(float));
		d.data_in = unused_buffer;
	}
	d.input_frames = f.nSamples;
	e = src_process(&c, &d);
	output_len = d.output_frames_gen;
	assert(e == 0);
	if (d.input_frames_used != f.nSamples)
	{
		int ubl = 2 * d.input_frames - d.input_frames_used;
		if (ubl > unused_buffer_len)
		{
			if (unused_buffer)
				delete unused_buffer;
			unused_buffer_len = ubl;
			unused_buffer = new float[unused_buffer_len];
		}
		unused_len = d.input_frames - d.input_frames_used;
		memcpy(unused_buffer, d.data_in + d.input_frames_used, unused_len * sizeof(float));
	}
	else
	{
		unused_len = 0;
	}
	return true;
}
uint32_t Resampler::Output(AudioFrame* f)
{
	if (output_len)
	{
		f->Allocate(output_len, buffer);
		output_len = 0;
		return 1;
	}
	return 0;
}

float Resampler::GetEstimateDelay()
{
	return  buffer_len / (float)output_format.sampleRate * 500.f;
}
