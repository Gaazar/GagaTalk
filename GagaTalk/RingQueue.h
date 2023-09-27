#pragma once
#include <mutex>
#include "IAudioFrameProcessor.h"
template<typename T>
class ring_queue
{


};


class ring_buffer
{
	AudioFrame* elements = nullptr;
	int size = 0;
	int read_pos = 0;
	int write_pos = 0;
	std::mutex m;
	//AudioFrame& operator[](int i);

	int index(int i)
	{
		while (i < 0) i += size;
		while (i >= size) i -= size;
		return i;
	}
public:
	ring_buffer(int size = 8)
	{
		this->size = size;
		elements = new AudioFrame[size];
	}
	bool read(AudioFrame* f)
	{
		std::lock_guard<std::mutex> g(m);
		if (read_pos == write_pos)
			return false;
		f->Allocate(elements[read_pos].nSamples, elements[read_pos].samples);
		read_pos = index(read_pos + 1);
		return true;
	}
	void write(AudioFrame* f)
	{
		std::lock_guard<std::mutex> g(m);
		elements[write_pos].Allocate(f->nSamples, f->samples);
		if (index(write_pos + 1) == read_pos)
		{
			read_pos = index(read_pos + 1);
		}
		write_pos = index(write_pos + 1);
	}
	bool is_full()
	{
		return index(write_pos + 1) == read_pos;
	}
	int get_size()
	{
		return size;
	}
	void resize(int size)
	{
		if (this->size == size) return;
		std::lock_guard<std::mutex> g(m);
		AudioFrame* new_buf = new AudioFrame[size];
		int mvsz = size > this->size ? this->size : size;
		memcpy(new_buf, elements, sizeof(AudioFrame) * mvsz);
		delete[] elements;
		elements = new_buf;
		this->size = size;
		write_pos = index(write_pos);
		read_pos = index(read_pos);
	}
};