#pragma once
#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>

class pipe_buf : public std::streambuf
{
	std::condition_variable producer, comsumer;
	std::mutex mtx_write;
	std::mutex mtx_swap;
	std::vector<char> rwbuf[2];
	int rbuf = 0;
	std::vector<char>& rb()
	{
		return rwbuf[rbuf % 2];
	}
	std::vector<char>& wb()
	{
		return rwbuf[(rbuf + 1) % 2];
	}
	void swap_wr()
	{
		rbuf = (rbuf + 1) % 2;
		rwbuf[(rbuf + 1) % 2].clear();
	}
protected:

	int underflow()
	{
		std::unique_lock<std::mutex> lck(mtx_write);
		producer.wait(lck, [this]()
			{
				return wb().size() > 0;
			});
		swap_wr();
		auto b = rb().data();
		setg(b, b, b + rb().size());
		return rb()[0];
	}
	//std::streamsize xsgetn(char* s, std::streamsize size)
	//{
	//	return 0;
	//}
	int overflow(int c)
	{
		if (c == EOF)
			return 0;
		char b = c;
		std::lock_guard<std::mutex> g(mtx_write);
		wb().push_back(b);
		producer.notify_all();
		return c;
	}
	std::streamsize xsputn(const char* s, std::streamsize size)
	{
		std::lock_guard<std::mutex> g(mtx_write);
		wb().reserve(wb().size() + size);
		for (int i = 0; i < size; i++)
			wb().push_back(s[i]);
		producer.notify_all();
		return size;
	}
	int sync() override {
		return 0;
	}
};