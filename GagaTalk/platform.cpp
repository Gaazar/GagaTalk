#define WIN32
#include "client.h"
#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <sapi.h>
#include <opus/opus.h>
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <initguid.h>
#include <assert.h>
#include "audio_process.h"
#include <functional>
#include <configor/json.hpp>
#include <fstream>
#include <mutex>
#include "audio_process.h"

#pragma comment(lib,"opus.lib")
#pragma comment(lib,"avrt.lib")

#define REFTIMES_PER_SEC  200000//25ms //10000000 = 1s
#define SAFE_RELEASE(x) x->Release()
#define CHECK_HR

IMMDeviceEnumerator* p_dev_enum;
std::vector<voice_playback*> p_playbacks;
std::vector<recorder_ref*> p_record_refs;
voice_recorder* p_recorder = nullptr;
recorder_ref* p_rec_lb_ref = nullptr;
voice_playback* p_pb_loopback = nullptr;
std::mutex p_m_rec_ref;
std::string p_outdev_id;
std::string p_indev_id;
float p_master_mul = 1.f;
bool p_master_mute = false;
bool p_master_silent = false;

int w2a(const wchar_t* in, int len, char** out)
{
	int acl = WideCharToMultiByte(CP_ACP, 0, in, len, NULL, 0, 0, 0);
	char* buffer = new char[acl + 1];
	//多字节编码转换成宽字节编码  
	WideCharToMultiByte(CP_ACP, 0, in, len, buffer, acl, 0, 0);
	buffer[acl] = '\0';             //添加字符串结尾  
	//删除缓冲区并返回值  
	*out = buffer;
	return acl;

}
int a2w(const char* in, int len, wchar_t** out)
{
	int wcl = MultiByteToWideChar(CP_ACP, 0, in, len, NULL, 0);
	TCHAR* buffer = new TCHAR[wcl + 1];
	//多字节编码转换成宽字节编码  
	MultiByteToWideChar(CP_ACP, 0, in, len, buffer, wcl);
	buffer[wcl] = '\0';             //添加字符串结尾  
	//删除缓冲区并返回值  
	*out = buffer;
	return wcl;
}
HRESULT CreateAudioClient(IMMDevice* pDevice, IAudioClient** ppAudioClient);
struct voice_recorder
{
	std::thread th_record;
	IAudioClient* aud_cli;
	IAudioCaptureClient* aud_in;
	//std::function<void(AudioFrame*)> callback;

	configor::json filter_config;
	std::vector<IAudioFrameProcessor*> filters;
	WAVEFORMATEX* wfmt;
	FrameAligner fa;
	Resampler* rsmplr = nullptr;
	bool discarded = false;

	voice_recorder();
	~voice_recorder();
	void on_aligned_pack(AudioFrame* af)
	{
		AudioFrame f;
		f.Allocate(af->nSamples, af->samples, af->nChannel);
		for (auto& i : filters)
		{
			switch (i->Type())
			{
			case AudioProcessType::IO:
				i->Input(f);
				i->Output(&f);
				if (f.nSamples != af->nSamples)
				{
					printf("AudioProcess middleware IO sample count conflicting.\n");
					return;
				}
				break;
			case AudioProcessType::Process:
				i->Process(f);
				break;
			default:
				break;
			}
		}
		//if (callback)
		//	callback(&f);
		{
			std::lock_guard<std::mutex> g(p_m_rec_ref);
			for (auto& i : p_record_refs)
			{
				if (i->callback)
					i->callback(&f);
			}
		}
		f.Release();
	}
	void create_devices(std::string devid)
	{
		IMMDevice* pEndpoint = NULL;
		HRESULT hr = 0;
		if (!devid.length())
			hr = p_dev_enum->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pEndpoint);
		else
		{
			wchar_t* wcs;
			a2w(devid.c_str(), devid.length(), &wcs);
			hr = p_dev_enum->GetDevice(wcs, &pEndpoint);
			delete wcs;
		}
		hr = CreateAudioClient(pEndpoint, &aud_cli);
		aud_cli->GetMixFormat(&wfmt);
		rsmplr = new Resampler(wfmt->nSamplesPerSec, 48000);
		fa = FrameAligner(480);
		if (wfmt->nChannels != 1)
		{
			printf("Multiple channel microphone is not supported yet, channel 1 will be used.\n");
		}
		//for (int i = 0; i < wfmt->nChannels; i++)
		//{
		//	rsmplr[i] = Resampler(wfmt->nSamplesPerSec, 48000);
		//}
		auto hev = CreateEvent(nullptr, false, false, nullptr);
		hr = aud_cli->SetEventHandle(hev);
		hr = aud_cli->GetService(IID_PPV_ARGS(&aud_in));
		discarded = false;
		th_record = std::thread([=]()
			{
				BYTE* pcm = nullptr;
				auto hr = aud_cli->Start();
				while (!terminated && !discarded)
				{
					UINT packsz;
					UINT numFramesAvailable;
					auto retv = WaitForSingleObject(hev, INFINITE);
					aud_in->GetNextPacketSize(&packsz);
					DWORD flags;
					if (packsz != 0)
					{
						hr = aud_in->GetBuffer(
							&pcm,
							&numFramesAvailable,
							&flags, NULL, NULL);

						if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || p_master_mute)
						{
							pcm = NULL;  // Tell CopyData to write silence.
						}
						if (pcm)
						{
							AudioFrame f;
							f.Allocate(numFramesAvailable, (float*)pcm, wfmt->nChannels);
							if (wfmt->nChannels > 1)
							{
								AudioFrame* f_c = new AudioFrame[wfmt->nChannels];
								ChannelUtil::Split(f, f_c);
								f.Release();
								f.Allocate(f_c[0].nSamples, f_c[0].samples);
								for (auto i = 0; i < wfmt->nChannels; i++)
								{
									f_c[i].Release();
								}
								delete[] f_c;
							}
							if (wfmt->nSamplesPerSec != 48000)
							{
								rsmplr->Input(f);
								f.Release();
								if (!rsmplr->Output(&f))
								{
									printf("Resampler Error\n");
									return;
								}
							}
							fa.Input(f);
							f.Release();
							while (fa.Output(&f))
							{
								on_aligned_pack(&f);
								f.Release();
							}
						}
						hr = aud_in->ReleaseBuffer(numFramesAvailable);
					}
				}
				aud_cli->Release();
				aud_in->Release();
				CloseHandle(hev);
			});
		pEndpoint->Release();
	}
	int delete_devices()
	{
		if (discarded)
			return -1;
		discarded = true;
		delete rsmplr;
		th_record.join();
		return 0;
	}
	void change_devices(std::string devid)
	{
		delete_devices();
		create_devices(devid);
	}
	void set_filter(std::string s);
};
struct voice_playback//auto convert channels and samplerate
{
	//voice data
	int channel;
	ring_buffer aud_buffer;

	//playback stuff
	std::thread th_play;
	IAudioClient* aud_cli;
	IAudioRenderClient* aud_out = nullptr;
	ISimpleAudioVolume* aud_vol = nullptr;
	OpusDecoder* aud_dec;
	WAVEFORMATEX* wfmt;
	FrameAligner fa;
	Resampler* rsmplr = nullptr;
	bool discarded = false;

public:
	float get_volume()
	{
		float v;
		auto hr = aud_vol->GetMasterVolume(&v);
		return mul_to_db(v);
	}
	float set_volume(float v)
	{
		v = db_to_mul(v);
		auto hr = aud_vol->SetMasterVolume(v, nullptr);
		assert(hr == S_OK);
		return v;
	}
	bool get_mute()
	{
		BOOL v;
		auto hr = aud_vol->GetMute((BOOL*)&v);
		return v;
	}
	bool set_mute(bool v)
	{
		auto hr = aud_vol->SetMute(v, nullptr);
		assert(hr == S_OK);
		return v;
	}
	void post_audio_frame(AudioFrame* af)
	{
		AudioFrame f;
		f.Allocate(af->nSamples, af->samples, af->nChannel);
		if (wfmt->nSamplesPerSec != 48000)
		{
			rsmplr->Input(f);
			f.Release();
			rsmplr->Output(&f);
		}
		fa.Input(f);
		f.Release();
		while (fa.Output(&f))
		{
			//std::cout << "audpak pop :" << __n_c << std::endl;
			//__n_c++;
			aud_buffer.write(&f);
			f.Release();
		}

	}
	void post_opus_pack(const char* buf, int sz)
	{
		float tmp_pcm[1920];
		AudioFrame f;
		int n_s = opus_decode_float(aud_dec, (unsigned char*)buf, sz, tmp_pcm, 1920, 0);
		f.Allocate(n_s, tmp_pcm);
		if (wfmt->nSamplesPerSec != 48000)
		{
			rsmplr->Input(f);
			f.Release();
			rsmplr->Output(&f);
		}
		fa.Input(f);
		f.Release();
		while (fa.Output(&f))
		{
			aud_buffer.write(&f);
			f.Release();
		}
	}
	void change_device(std::string device_id);
	int create_devices(std::string device_id);
	int delete_devices();
	void reset_codec()
	{
		int ec = 0;
		auto prev = aud_dec;
		aud_dec = opus_decoder_create(48000, 1, &ec);
		opus_decoder_destroy(prev);
	}
	voice_playback();
	~voice_playback();
};
int platform_init()
{
	if (FAILED(::CoInitialize(NULL)))
		return FALSE;
	auto hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
		IID_PPV_ARGS(&p_dev_enum));
	if (!SUCCEEDED(hr)) return -1;

	socket_init();

}

voice_playback* plat_create_vpb()
{
	//printf("+vpb\n");
	auto vp = new voice_playback();
	p_playbacks.push_back(vp);
	return vp;
}
void plat_delete_vpb(voice_playback* vp)
{
	for (auto& i : p_playbacks)
	{
		if (i == vp)
			i = nullptr;
	}
	//printf("-vpb\n");
	delete vp;
}

bool plat_set_input_device(std::string devid)
{
	IMMDevice* d = nullptr;
	wchar_t* wd;
	a2w(devid.c_str(), devid.length(), &wd);
	p_dev_enum->GetDevice(wd, &d);
	delete wd;
	if (d)
	{
		d->Release();
		p_indev_id = devid;
		if (p_recorder)
			p_recorder->change_devices(p_indev_id);
		return true;
	}
	return false;
}
bool plat_set_output_device(std::string devid)
{
	IMMDevice* d = nullptr;
	wchar_t* wd;
	a2w(devid.c_str(), devid.length(), &wd);
	p_dev_enum->GetDevice(wd, &d);
	delete wd;
	if (d)
	{
		d->Release();
		p_outdev_id = devid;
		for (auto& i : p_playbacks)
		{
			if (i)
			{
				i->change_device(p_outdev_id);
			}
		}
		return true;
	}
	return false;

}
HRESULT CreateAudioClient(IMMDevice* pDevice, IAudioClient** ppAudioClient)
{
	if (!pDevice)
	{
		return E_INVALIDARG;
	}

	if (!ppAudioClient)
	{
		return E_POINTER;
	}

	HRESULT hr = S_OK;

	WAVEFORMATEX* pwfx;

	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;

	UINT32 nFrames = 0;

	IAudioClient* pAudioClient = NULL;

	// Get the audio client.
	CHECK_HR(hr = pDevice->Activate(
		__uuidof(IAudioClient),
		CLSCTX_ALL,
		NULL,
		(void**)&pAudioClient));

	// Get the device format.
	CHECK_HR(hr = pAudioClient->GetMixFormat(&pwfx));

	// Open the stream and associate it with an audio session.
	hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		hnsRequestedDuration,
		0,
		(WAVEFORMATEX*)pwfx,
		NULL);

	// If the requested buffer size is not aligned...
	if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
	{
		// Get the next aligned frame.
		CHECK_HR(hr = pAudioClient->GetBufferSize(&nFrames));
		hnsRequestedDuration = (REFERENCE_TIME)
			((10000.0 * 1000 / ((WAVEFORMATEX*)pwfx)->nSamplesPerSec * nFrames) + 0.5);

		// Release the previous allocations.
		SAFE_RELEASE(pAudioClient);
		CoTaskMemFree(&pwfx);

		// Create a new audio client.
		CHECK_HR(hr = pDevice->Activate(
			__uuidof(IAudioClient),
			CLSCTX_ALL,
			NULL,
			(void**)&pAudioClient));

		// Get the device format.
		//CHECK_HR(hr = pAudioClient->GetMixFormat(&pwfx));

		// Open the stream and associate it with an audio session.
		CHECK_HR(hr = pAudioClient->Initialize(
			AUDCLNT_SHAREMODE_EXCLUSIVE,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			hnsRequestedDuration,
			hnsRequestedDuration,
			(WAVEFORMATEX*)pwfx,
			NULL));
	}
	else
	{
		CHECK_HR(hr);
	}

	// Return to the caller.
	*(ppAudioClient) = pAudioClient;
	(*ppAudioClient)->AddRef();

done:

	// Clean up.
	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pAudioClient);
	return hr;
}

recorder_ref* plat_create_vr()
{
	std::lock_guard<std::mutex> g(p_m_rec_ref);
	auto ref = new recorder_ref();
	p_record_refs.push_back(ref);
	if (!p_recorder)
	{
		auto vr = new voice_recorder();
		p_recorder = vr;
		std::string f;
		conf_get_filter(f);
		plat_set_filter(f);
	}
	return ref;

}
void plat_delete_vr(recorder_ref* v)
{
	recorder_ref* del = nullptr;
	{
		std::lock_guard<std::mutex> g(p_m_rec_ref);
		for (auto i = p_record_refs.begin(); i != p_record_refs.end(); i++)
		{
			if (*i == v)
			{
				p_record_refs.erase(i);
				del = v;
				//delete v;
				break;
			}
		}
	}
	if (!p_record_refs.size())
	{
		delete p_recorder;
		p_recorder = nullptr;
	}
	if (del)
		delete del;
}

int voice_playback::create_devices(std::string device_id)
{
	IMMDevice* pEndpoint = NULL;
	HRESULT hr = 0;
	if (device_id.length())
	{
		wchar_t* wcs;
		a2w(device_id.c_str(), device_id.length(), &wcs);
		hr = p_dev_enum->GetDevice(wcs, &pEndpoint);
		delete wcs;
	}
	else
		hr = p_dev_enum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pEndpoint);
	hr = CreateAudioClient(pEndpoint, &aud_cli);
	aud_cli->GetMixFormat(&wfmt);
	rsmplr = new  Resampler(48000, wfmt->nSamplesPerSec);
	auto hev = CreateEvent(nullptr, false, false, nullptr);
	hr = aud_cli->SetEventHandle(hev);
	hr = aud_cli->GetService(IID_PPV_ARGS(&aud_out));
	hr = aud_cli->GetService(IID_PPV_ARGS(&aud_vol));
	discarded = false;
	th_play = std::thread([this, hev]()
		{
			BYTE* pcm = nullptr;
			auto hr = aud_cli->Start();
			//auto hmmt = AvSetMmThreadCharacteristics(L"Audio", &task_index);
			while (!discarded)
			{
				auto retv = WaitForSingleObject(hev, INFINITE);
				//0x88890006 
				//constexpr bool b = 0x88890006 == AUDCLNT_E_BUFFER_TOO_LARGE;
				AUDCLNT_E_BUFFER_TOO_LARGE;
				unsigned full = 0, padding = 0;
				AudioFrame f;
				if (aud_buffer.read(&f) && !p_master_silent)
				{
					hr = aud_cli->GetBufferSize(&full);
					assert(hr == S_OK);
					hr = aud_cli->GetCurrentPadding(&padding);
					assert(hr == S_OK);
					int frames = full - padding;
					hr = aud_out->GetBuffer(frames, &pcm);
					assert(hr == S_OK);
					int min_smp = min(f.nSamples, frames);
					for (int i = 0; i < min_smp; i++)
					{
						for (int c = 0; c < wfmt->nChannels; c++)
						{
							((float*)pcm)[i * wfmt->nChannels + c] = f.samples[i] * p_master_mul;
						}
					}

					hr = aud_out->ReleaseBuffer(f.nSamples, 0);
					assert(hr == S_OK);
					f.Release();
				}
				else
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			}
			aud_cli->Stop();
			aud_out->Release();
			aud_cli->Release();
			aud_cli = nullptr;
			aud_out = nullptr;
			CloseHandle(hev);
		});
	//ac->Start();
	hr = pEndpoint->Release();
	return hr;
}
int voice_playback::delete_devices()
{
	if (discarded) return -1;
	discarded = true;
	if (rsmplr)
		delete rsmplr;
	th_play.join();
	return 0;
}
voice_playback::voice_playback() : fa(480), aud_buffer(16)
{
	int ec = 0;
	aud_dec = opus_decoder_create(48000, 1, &ec);
	assert(aud_dec);

	ec = create_devices(p_outdev_id);
	assert(ec == S_OK);
}
voice_playback::~voice_playback()
{
	delete_devices();
}
void connection::on_recv_voip_pack(const char* buffer, int len)
{
	/*
4byte:	suid
...		payload
*/
	if (len < 4) return;
	uint32_t rid = *(uint32_t*)buffer;
	if (entities.count(rid))
	{
		if (entities[rid]->playback)
			entities[rid]->playback->post_opus_pack(buffer + 4, len - 4);
	}


}
void voice_playback::change_device(std::string device_id)
{
	delete_devices();
	create_devices(device_id);
}
float entity::get_volume()
{
	return playback->get_volume();
}
float entity::set_volume(float v)
{
	return playback->set_volume(v);
}
bool entity::get_mute()
{
	return playback->get_mute();
}
bool entity::set_mute(bool m)
{
	return playback->set_mute(m);
}
void entity::remove_playback()
{
	if (playback)
		plat_delete_vpb(playback);
	playback = nullptr;
}
void entity::create_playback()
{
	playback = plat_create_vpb();
	load_profile();
}
voice_playback* entity::move_playback()
{
	auto ret = playback;
	playback = nullptr;
	return playback;
}
voice_recorder::voice_recorder() : fa(480)
{
	create_devices(p_indev_id);
}
voice_recorder::~voice_recorder()
{
	delete_devices();
}
void voice_recorder::set_filter(std::string s)
{
	for (auto& i : filters)
	{
		delete i;
	}
	filters.clear();
	filter_config = configor::json::parse(s);
	for (auto i : filter_config)
	{
		if (!i.count("enable") || !i.count("type") || !i.count("args")) continue;
		if (!i["enable"].as_bool()) continue;
		auto args = i["args"];
		std::string t = i["type"].as_string();
		if (t == "RNNDenoise")
		{
			RNNDenoise* rnn = new RNNDenoise();
			filters.push_back(rnn);
		}
		else if (t == "Compressor")
		{
			Compressor* c = new Compressor();
			if (args.count("gain"))
				c->Gain(args["gain"].as_float());
			if (args.count("ratio"))
				c->Ratio(args["ratio"].as_float());
			if (args.count("threshold"))
				c->Threshold(args["threshold"].as_float());
			if (args.count("attack"))
				c->AttackTime(args["attack"].as_float());
			if (args.count("release"))
				c->ReleaseTime(args["release"].as_float());
			filters.push_back(c);
		}
		else if (t == "Gain&Clamp")
		{
			GainClamp* c = new GainClamp();
			if (args.count("gain"))
				c->Gain(args["gain"].as_float());
			if (args.count("clamp"))
				c->Clamp(args["clamp"].as_float());
			filters.push_back(c);
		}
	}
}

void enable_voice_loopback()
{
	if (!p_pb_loopback)
		p_pb_loopback = plat_create_vpb();
	p_rec_lb_ref = plat_create_vr();

	p_rec_lb_ref->callback = [](AudioFrame* f)
	{
		p_pb_loopback->post_audio_frame(f);
	};
}
void disable_voice_loopback()
{
	plat_delete_vr(p_rec_lb_ref);
	if (p_pb_loopback)
		plat_delete_vpb(p_pb_loopback);
	p_pb_loopback = nullptr;

}
bool plat_set_filter(std::string filter)
{
	if (!p_recorder) return false;
	p_recorder->set_filter(filter);
}
float plat_set_global_volume(float db)
{
	p_master_mul = db_to_mul(db);

	return db;
}
bool plat_set_global_mute(bool m)
{
	p_master_mute = m;
	return p_master_mute;
}
bool plat_set_global_silent(bool m)
{
	p_master_silent = m;
	return p_master_silent;
}
float plat_get_global_volume()
{
	return mul_to_db(p_master_mul);
}
bool plat_get_global_mute()
{
	return p_master_mute;
}
bool plat_get_global_silent()
{
	return p_master_silent;
}


uint64_t randu64()
{
	uint64_t v = 0;
	for (int i = 0; i < 8; i++)
	{
		v |= ((uint64_t)rand() % 0xFF) << (i * 8);
	}
	return v;
}
uint32_t randu32()
{
	uint64_t v = 0;
	for (int i = 0; i < 4; i++)
	{
		v |= ((uint64_t)rand() % 0xFF) << (i * 8);
	}
	return v;
}