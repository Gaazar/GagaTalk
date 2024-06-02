#include "IAudioFrameProcessor.h"
#include <assert.h>
FrameAligner::FrameAligner() : FrameAligner(480, 1)
{

}
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
//FrameAligner& FrameAligner::operator =(const FrameAligner& f)
//{
//	nBuffered = 0;
//	nChannel = f.nChannel;
//	target_size = f.target_size;
//	buffer = new float[target_size * nChannel];
//	return *this;
//}
void FrameAligner::Resize(uint32_t target_frame_size, uint32_t channel_count)
{
	if (buffer) delete[] buffer;
	nBuffered = 0;
	nChannel = channel_count;
	target_size = target_frame_size;
	buffer = new float[target_size * nChannel];

}
bool FrameAligner::Input(AudioFrame& in_f)
{
	assert(in_f.nChannel == nChannel);
	const int nf = in_f.nChannel * in_f.nSamples;
	const int tnf = target_size * nChannel;
	const int nbs = sizeof in_f.samples[0];
	const int nc = nChannel;
	if (nBuffered + nf >= tnf)
	{
		//缓冲区和新的组的尺寸大于或等于一个输出组的长度，能够产生输出组
		int ob = 0;
		int oi = 0;
		while (nBuffered + nf >= tnf + ob + oi)
		{
			//能够继续产生新的输出组
			frames.emplace_back();
			frames[frames.size() - 1].Allocate(target_size, nullptr, nChannel);
			AudioFrame& f = frames[frames.size() - 1];
			if (nBuffered)
			{
				//缓冲区有数据，先将其中的数据添加进输出组
				ob = nBuffered;
				memcpy(f.samples, buffer, nBuffered * nbs);
			}
			//将输入组的数据拷贝到输出组
			memcpy(f.samples + ob * nc, in_f.samples + oi * nc, (target_size - ob) * nbs);
			oi += target_size - ob;
			ob = 0;
			nBuffered = 0;
		}
		nBuffered = in_f.nSamples - oi;
		memcpy(buffer, in_f.samples + oi * nc, nBuffered * nbs);
	}
	else
	{
		//缓冲区和新的组的尺寸小于一个输出组的长度，不能输出组，缓存到缓冲区等待和下一组拼接
		memcpy(buffer + nBuffered * nc, in_f.samples, nf * nbs);
		nBuffered += nf;
	}
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

