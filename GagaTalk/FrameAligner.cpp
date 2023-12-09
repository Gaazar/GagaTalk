#include "IAudioFrameProcessor.h"
#include <assert.h>
FrameAligner::FrameAligner(uint32_t target_frame_size, uint32_t channel_count)
{
	nBuffered = 0;
	nChannel = channel_count;
	target_size = target_frame_size;
	buffer = new float[target_frame_size * channel_count];
	//printf("newfa %p\t%u,%u\n", this, target_frame_size, channel_count);
}
FrameAligner::~FrameAligner()
{
	if (buffer)
		delete[] buffer;
	buffer = nullptr;
	//printf("delfa %p\t%u,%u\n", this, target_size, nChannel);
}
FrameAligner::FrameAligner(const FrameAligner& f)
{
	nBuffered = 0;
	nChannel = f.nChannel;
	target_size = f.target_size;
	buffer = new float[target_size * nChannel];
}
FrameAligner::FrameAligner(FrameAligner&& f) noexcept
{
	nBuffered = 0;
	nChannel = f.nChannel;
	target_size = f.target_size;
	buffer = f.buffer;
	f.buffer = nullptr;
}
FrameAligner& FrameAligner::operator =(const FrameAligner& f)
{
	nBuffered = 0;
	nChannel = f.nChannel;
	target_size = f.target_size;
	buffer = new float[target_size * nChannel];
	return *this;
}
bool FrameAligner::Input(AudioFrame& in_f)
{
	assert(in_f.nChannel == nChannel);
	int n = 1;
	int o = 0;
	while (in_f.nSamples + nBuffered >= target_size + o)
	{
		frames.push_back({});
		frames[frames.size() - 1].Allocate(target_size, nullptr, nChannel);
		AudioFrame& f = frames[frames.size() - 1];
		if (nBuffered)
			memcpy(f.samples, buffer, sizeof(float) * nBuffered * nChannel);
		memcpy(f.samples + nBuffered * nChannel, in_f.samples + o, sizeof(float) * (target_size - nBuffered) * nChannel);
		o += target_size - nBuffered;
		n++;
		nBuffered = 0;
	}
	nBuffered = in_f.nSamples - o;
	memcpy(buffer, in_f.samples + o * nChannel, sizeof(float) * nBuffered * nChannel);
	//assert(nBuffered * nChannel <= target_size * nChannel);

	return true;
}
uint32_t FrameAligner::Output(AudioFrame* f)
{
	uint32_t sz = frames.size();
	if (sz)
	{
		f->Allocate(frames[0].nSamples, frames[0].samples);
		frames[0].Release();
		frames.erase(frames.begin());
	}
	return sz;
}

uint32_t FrameAligner::GetFrameSize()
{
	return target_size;
}

