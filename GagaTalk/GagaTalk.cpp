// GagaTalk.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#define WIN32
#include <iostream>
#include <stdio.h>
#include <sapi.h>
#include <opus/opus.h>
#include <MMDeviceAPI.h>
#include <AudioClient.h>

#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <initguid.h>
#include <fstream>
#include <thread>
#include <assert.h>
#include "RingQueue.h"
#include "audio_process.h"
#include "IAudioFrameProcessor.h"
#include "gt_defs.h"
#include "sqlite/sqlite3.h"
#include "client.h"
#include "DbgHelp.h"

#pragma comment(lib,"opus.lib")
#pragma comment(lib,"avrt.lib")


#define REFTIMES_PER_SEC  200000//25ms //10000000 = 1s
#define SAFE_RELEASE(x) x->Release()
#define CHECK_HR
IMMDeviceEnumerator* pEnumerator;

float records[48000 * 5];// 5sec
float records_rs[48000 * 5];// 5sec
float records_dn[48000 * 5];// 5sec
float records_dec[48000 * 5];// 5sec
int rec_counter = 0;
UINT bufferFrameCount = 0;
ring_buffer rec_rb(128);
ring_buffer rec_rb_r(128);
ring_buffer rec_f(128);
std::ofstream raw_out_file;
int shell_main();
bool terminated = false;
std::thread g_th_mic;
bool g_mic_terminated = true;
#pragma comment( lib, "Dbghelp.lib" )
int GenerateMiniDump(PEXCEPTION_POINTERS pExceptionPointers)
{
	// 定义函数指针

	// 创建 dmp 文件件
	TCHAR szFileName[MAX_PATH] = { 0 };
	const TCHAR* szVersion = L"gtdumpv0.8";
	SYSTEMTIME stLocalTime;
	GetLocalTime(&stLocalTime);
	wsprintf(szFileName, L"%s-%04d%02d%02d-%02d%02d%02d.dmp",
		szVersion, stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
		stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond);
	HANDLE hDumpFile = CreateFile(szFileName, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	if (INVALID_HANDLE_VALUE == hDumpFile)
	{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	// 写入 dmp 文件
	MINIDUMP_EXCEPTION_INFORMATION expParam;
	expParam.ThreadId = GetCurrentThreadId();
	expParam.ExceptionPointers = pExceptionPointers;
	expParam.ClientPointers = FALSE;
	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
		hDumpFile, MiniDumpWithDataSegs, (pExceptionPointers ? &expParam : NULL), NULL, NULL);
	// 释放文件
	CloseHandle(hDumpFile);
	return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI ExceptionFilter(LPEXCEPTION_POINTERS lpExceptionInfo)
{
	// 这里做一些异常的过滤或提示
	if (IsDebuggerPresent())
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}
	return GenerateMiniDump(lpExceptionInfo);
}

HRESULT CreateAudioClient(IMMDevice* pDevice, IAudioClient** ppAudioClient);
void Play(IMMDeviceEnumerator* enumerator, float* l, float* r)
{
	IPropertyStore* prop;
	PROPVARIANT v;
	IMMDevice* pEndpoint = NULL;
	auto hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pEndpoint);
	PropVariantInit(&v);
	pEndpoint->OpenPropertyStore(STGM_READ, &prop);
	prop->GetValue(PKEY_Device_FriendlyName, &v);
	printf("default communication output = %S\n", v.pwszVal);
	PropVariantClear(&v);
	prop->Release();

	IAudioClient* ac;
	hr = CreateAudioClient(pEndpoint, &ac);
	auto hev = CreateEvent(nullptr, false, false, nullptr);
	hr = ac->SetEventHandle(hev);
	IAudioRenderClient* ao;
	hr = ac->GetService(IID_PPV_ARGS(&ao));
	BYTE* pcm = nullptr;
	uint64_t timecode = 0;
	hr = ac->Start();
	bool stoped = false;
	DWORD task_index = 0;
	auto hmmt = AvSetMmThreadCharacteristics(L"Audio", &task_index);
	while (!stoped)
	{
		auto retv = WaitForSingleObject(hev, INFINITE);
		//0x88890006 
		//constexpr bool b = 0x88890006 == AUDCLNT_E_BUFFER_TOO_LARGE;
		AUDCLNT_E_BUFFER_TOO_LARGE;
		unsigned full = 0, padding = 0;
		hr = ac->GetBufferSize(&full);
		hr = ac->GetCurrentPadding(&padding);
		int frames = full - padding;
		int buffer_size = frames * (32 / 8) * 2;
		hr = ao->GetBuffer(frames, &pcm);
		for (int i = 0; i < frames; i++)
		{
			float t = (timecode + i) / 48000.f;
			//((float*)pcm)[i * 2] = sin(200 * 6.28f * t) * 0.2;
			//((float*)pcm)[i * 2 + 1] = cos(300 * 6.28f * t) * 0.2;
			if (timecode + i >= 48000 * 5)
			{
				stoped = true;
				break;
			}
			((float*)pcm)[i * 2] = l[timecode + i];
			((float*)pcm)[i * 2 + 1] = r[timecode + i];
		}
		timecode += frames;
		hr = ao->ReleaseBuffer(frames, 0);
	}
	//ac->Start();
	ac->Stop();
	ao->Release();
	ac->Release();
	pEndpoint->Release();

}
void Record(IMMDeviceEnumerator* enumerator)
{
	IPropertyStore* prop;
	PROPVARIANT v;
	IMMDevice* pEndpoint = NULL;
	auto hr = enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &pEndpoint);
	PropVariantInit(&v);
	pEndpoint->OpenPropertyStore(STGM_READ, &prop);
	prop->GetValue(PKEY_Device_FriendlyName, &v);
	printf("default communication input = %S\n", v.pwszVal);
	PropVariantClear(&v);
	prop->Release();

	IAudioClient* ac;
	hr = CreateAudioClient(pEndpoint, &ac);
	auto hev = CreateEvent(nullptr, false, false, nullptr);
	hr = ac->SetEventHandle(hev);
	IAudioCaptureClient* ai;
	hr = ac->GetService(IID_PPV_ARGS(&ai));

	BYTE* pcm = nullptr;
	bool stoped = false;
	hr = ac->Start();

	while (!stoped)
	{
		UINT packsz;
		UINT numFramesAvailable;
		ai->GetNextPacketSize(&packsz);
		DWORD flags;
		while (packsz != 0 && !stoped)
		{
			auto retv = WaitForSingleObject(hev, INFINITE);
			hr = ai->GetBuffer(
				&pcm,
				&numFramesAvailable,
				&flags, NULL, NULL);

			if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
			{
				pcm = NULL;  // Tell CopyData to write silence.
			}
			if (pcm)
			{
				for (auto i = 0; i < numFramesAvailable; ++i)
				{
					if (rec_counter + i >= 44100 * 5)
					{
						stoped = true;
						break;
					}
					records_rs[rec_counter + i] = ((float*)pcm)[i];

				}
				rec_counter += numFramesAvailable;
			}
			hr = ai->ReleaseBuffer(numFramesAvailable);
			hr = ai->GetNextPacketSize(&packsz);
		}
	}
	ac->Stop();
	ai->Release();
	ac->Release();
}
void ReadFileQueued(char* file_name)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	std::ifstream fs;
	fs.open(file_name, std::ios::binary);
	AudioFrame f;
	RNNDenoise rnnd;
	Compressor cps(48000);
	cps.Gain(10);
	cps.Ratio(6);
	float pcm[480];
	f.Allocate(480);
	int n_c = 0;
	while (!fs.eof())
	{
		fs.read((char*)pcm, sizeof(float) * 480);
		f.Allocate(480, (float*)pcm);
		//rnnd.Process(f);
		//cps.Process(f);
		//ChannelUtil::Clamp(f, 0);
		//rec_f.write(&f);
		f.Release();
		//if (rec_f.is_full())
		if (n_c % 8 == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10 * 7));
			//std::cout << "audpak push:" << n_c << std::endl;
		}
		n_c++;
	}
	f.Release();
	fs.close();

}
void RecordQueued(IMMDeviceEnumerator* enumerator)
{
	IPropertyStore* prop;
	PROPVARIANT v;
	IMMDevice* pEndpoint = NULL;
	auto hr = enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &pEndpoint);

	IAudioClient* ac;
	hr = CreateAudioClient(pEndpoint, &ac);
	auto hev = CreateEvent(nullptr, false, false, nullptr);
	hr = ac->SetEventHandle(hev);
	IAudioCaptureClient* ai;
	hr = ac->GetService(IID_PPV_ARGS(&ai));

	BYTE* pcm = nullptr;
	bool stoped = false;
	hr = ac->Start();
	FrameAligner fa(480);
	RNNDenoise rnnd;
	while (!stoped)
	{
		UINT packsz;
		UINT numFramesAvailable;
		auto retv = WaitForSingleObject(hev, INFINITE);
		ai->GetNextPacketSize(&packsz);
		DWORD flags;
		if (packsz != 0)
		{
			hr = ai->GetBuffer(
				&pcm,
				&numFramesAvailable,
				&flags, NULL, NULL);

			if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
			{
				pcm = NULL;  // Tell CopyData to write silence.
			}
			if (pcm)
			{
				AudioFrame f;
				AudioFrame fc;
				f.Allocate(numFramesAvailable, (float*)pcm);
				fa.Input(f);
				f.Release();
				while (fa.Output(&fc))
				{
					rec_rb_r.write(&fc);
					if (raw_out_file.good())
					{
						raw_out_file.write((char*)fc.samples, fc.nSamples * fc.nChannel * sizeof(float));
					}
					rnnd.Process(fc);
					rec_rb.write(&fc);
					fc.Release();
				}
			}
			hr = ai->ReleaseBuffer(numFramesAvailable);
		}
	}
	ac->Stop();
	ai->Release();
	ac->Release();

}
void PlayQueued(IMMDeviceEnumerator* enumerator, ring_buffer* buffer)
{
	IPropertyStore* prop;
	PROPVARIANT v;
	IMMDevice* pEndpoint = NULL;
	auto hr = enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pEndpoint);

	IAudioClient* ac;
	hr = CreateAudioClient(pEndpoint, &ac);
	auto hev = CreateEvent(nullptr, false, false, nullptr);
	hr = ac->SetEventHandle(hev);
	IAudioRenderClient* ao;
	hr = ac->GetService(IID_PPV_ARGS(&ao));
	BYTE* pcm = nullptr;
	uint64_t timecode = 0;
	hr = ac->Start();
	DWORD task_index = 0;
	int tc = 0;
	//auto hmmt = AvSetMmThreadCharacteristics(L"Audio", &task_index);
	while (!terminated)
	{
		auto retv = WaitForSingleObject(hev, INFINITE);
		//0x88890006 
		//constexpr bool b = 0x88890006 == AUDCLNT_E_BUFFER_TOO_LARGE;
		AUDCLNT_E_BUFFER_TOO_LARGE;
		unsigned full = 0, padding = 0;
		AudioFrame f;
		AudioFrame fr;
		if (buffer->read(&f))
		{
			tc++;
			hr = ac->GetBufferSize(&full);
			assert(hr == S_OK);
			hr = ac->GetCurrentPadding(&padding);
			assert(hr == S_OK);
			int frames = full - padding;
			hr = ao->GetBuffer(frames, &pcm);
			assert(hr == S_OK);
			if (f.nSamples <= frames)
			{
				for (int i = 0; i < f.nSamples; i++)
				{
					((float*)pcm)[i * 2] = f.samples[i];
					((float*)pcm)[i * 2 + 1] = f.samples[i];
				}
			}
			else
			{
				printf("ring buffer write too large.\n");
			}
			hr = ao->ReleaseBuffer(f.nSamples, 0);
			assert(hr == S_OK);
			f.Release();
			fr.Release();
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	}
	//ac->Start();
	ac->Stop();
	ao->Release();
	ac->Release();
	pEndpoint->Release();

}
float square(float t, float f, float p)
{
	if (t - floor(t) < 0.6) return 1;
	return 0;
}
void write_pcm_to_file(float* data, int sample_count, char* file_name)
{
	std::ofstream fs;
	fs.open(file_name, std::ios::binary);
	fs.write((char*)data, sizeof(float) * sample_count);
	fs.close();
}
int main()
{
	SetUnhandledExceptionFilter(ExceptionFilter);
	setlocale(LC_ALL, "zh-CN");
	ISpVoice* pVoice = NULL;
	int erc = 0;
	auto pEnc = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &erc);
	auto pDec = opus_decoder_create(48000, 1, &erc);

	srand(time(nullptr));
	if (FAILED(::CoInitialize(NULL)))
		return FALSE;
	HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice);
	//if (SUCCEEDED(hr))
	//{
	//    pVoice->SetRate(5);
	//    hr = pVoice->Speak(L"Hello world你好。嘎", 0, NULL);
	//    pVoice->Release();
	//    pVoice = NULL;
	//}
	std::cout << "Gagagaga\n";
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
		IID_PPV_ARGS(&pEnumerator));
	if (!SUCCEEDED(hr)) return -2;
	IMMDeviceCollection* pCollection = NULL;
	hr = pEnumerator->EnumAudioEndpoints(
		eAll,  // data-flow direction (input parameter) 
		DEVICE_STATE_ACTIVE,
		&pCollection);  // release interface when done 
	IMMDevice* pEndpoint = NULL;
	UINT counts = 0;
	pCollection->GetCount(&counts);
	LPWSTR ws;
	PROPVARIANT v;
	IPropertyStore* prop;
	for (auto i = 0; i < counts; ++i)
	{
		hr = pCollection->Item(i, &pEndpoint); //device Index Value
		pEndpoint->OpenPropertyStore(STGM_READ, &prop);
		pEndpoint->GetId(&ws);

		PropVariantInit(&v);
		prop->GetValue(PKEY_Device_FriendlyName, &v);
		printf("%S\n\t%S\n", ws, v.pwszVal);
		PropVariantClear(&v);
		printf("----------\n");

		prop->Release();
		CoTaskMemFree(ws);
		pEndpoint->Release();
	}
	char filen[] = "test_float_48k.raw";
	//std::thread thread_read(ReadFileQueued, filen);
	//raw_out_file.open(filen, std::ios::binary);

	//std::thread thread_rec(RecordQueued, pEnumerator);
	//Sleep(1000);
	//std::thread thread_play(PlayQueued, pEnumerator, &rec_rb);
	//sqlite3* sql;
	//hr = sqlite3_open("config.db", &sql);
	//std::thread thread_play2(PlayQueued, pEnumerator, &rec_f);
	configs_init();
	shell_main();
	//raw_out_file.close();

	return 0;
	printf("Recording\n");
	//Record(pEnumerator);
	for (int i = 0; i < 48000 * 5; i++)
	{
		float t = i / 44100.f;
		records_rs[i] = 0.7 *
			(0.5 * (sin(200 * 6.28 * t) +
				0.3 * cos(300 * 6.28 * t * (0.8 + 0.1 * sin(t)) + sin(15 * 6.28 * t)) +
				0.5 * sin((20 + 19980 * t / 5.f) * 6.28 * t + 1)
				)  //signal
				+ 0.001 * (rand() % 100) / 100.f) //noise
			* 0.3;
		//records_rs[i] = (sin(2000 * 6.28 * t) //signal
		//	+ 0.00 * (rand() % 100) / 100.f) //noise
		//	* 0.15;

	}
	Resampler lrs(44100, 48000, Resampler::QUALITY::SINC_BEST);
	float testf[480] = { 0 };
	AudioFrame af;
	AudioFrame bf;
	af.Allocate(441);
	//af.Allocate(560);
	int o = 0;
	for (int i = 0; i < 48000 * 5 / 441; i++)
	{
		//up sample
		memcpy(af.samples, &records_rs[i * 441], sizeof(float) * 441);
		lrs.Input(af);
		if (lrs.Output(&bf))
		{
			memcpy(&records[o], bf.samples, sizeof(float) * bf.nSamples);
			o += bf.nSamples;
		}

		//downsample
		//memcpy(af.samples, &records_rs[i * 560], sizeof(float) * 560);
		//ldrs.Input(af);
		//if (ldrs.Output(&bf))
		//{
		//	memcpy(&records[480 * i], bf.samples, sizeof(float) * 480);
		//}

	}
	//write_pcm_to_file(records_rs, 32000 * 5, (char*)"raw32k.raw");
	//write_pcm_to_file(records, 48000 * 5, (char*)"sinc44.1k-48k.raw");
	//write_pcm_to_file(records, 48000 * 5, (char*)"sinc_56k-48k.raw");
	//return 0;
	printf("Denoising\n");
	//for (int i = 0; i < 48000 * 5 / 480; i++)
	//{
	//	auto f = rnnoise_process_frame(dn, &records_dn[i * 480], &records[i * 480]);
	//}
	//rnnoise_destroy(dn);

	printf("Playing raw\n");
	Play(pEnumerator, records, records);

	printf("Playing denoised\n");
	Play(pEnumerator, records_dn, records_dn);

	printf("Encode Decode\n");

	int fsz = 480; //10ms
	unsigned char* pak = new unsigned char[1480];
	int d = 0;
	int tl = 0;
	int loss = 0;
	srand(GetProcessId(0));
	for (int i = 0; i < 48000 * 5 / fsz; i++)
	{
		auto len = opus_encode_float(pEnc, &records_dn[i * fsz], fsz, pak, 1480);
		tl += len;
		if (rand() % 100 < loss)
		{
			d += 480;
			continue;
		}

		auto fs = opus_decode_float(pDec, pak, len, records_dec + d, 9600, 0);
		d += fs;
	}
	printf("L=Denoised R=Denoised Codeced, data size = %d byte, bitrate = %fkb/s\n", tl, tl / 5.f / 1024.f);
	Play(pEnumerator, records_dec, records_dec);

	pEnumerator->Release();

	std::cin >> counts;
	::CoUninitialize();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
