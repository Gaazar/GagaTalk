#include "events.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <assert.h>
#include <sapi.h>
#include <configor/json.hpp>
#include <string>
using namespace configor;

std::vector<std::string> sa_queue;
std::mutex sa_m_q;
ISpVoice* sa_pVoice = nullptr;
std::thread sa_th_speak;
bool sa_discard = true;
json sa_profile = {};
bool sa_disable = false;
int sapi_say(std::string s)
{
	if (sa_disable) return 0;
	std::lock_guard<std::mutex> _g(sa_m_q);
	sa_queue.push_back(s);
	return 0;
}
void create_speak()
{
	if (!sa_discard) return;
	sa_discard = false;
	HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&sa_pVoice);
	assert(hr == S_OK);
	long l = 0;
	USHORT u = 0;
	sa_profile["disabled"] = false;
	sa_pVoice->GetRate(&l);
	sa_profile["rate"] = l;
	sa_pVoice->GetVolume(&u);
	sa_profile["voulme"] = u;
	sa_th_speak = std::thread([]()
		{
			CoInitialize(NULL);
			while (!sa_discard)
			{
				if (sa_queue.size())
				{
					wchar_t* wcs;
					{
						std::lock_guard<std::mutex> _g(sa_m_q);
						a2w(sa_queue[0].c_str(), sa_queue[0].length(), &wcs);
						sa_queue.erase(sa_queue.begin());
					}
					auto hr = sa_pVoice->Speak(wcs, 0, NULL);
					delete wcs;
				}
				else
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
				}

			}
			CoUninitialize();
		});
}
void sapi_set_profile(std::string s)
{
	json j = json::parse(s);
	if (j["rate"].is_number())
	{
		sapi_set_rate(j["rate"].as_integer());
	}
	if (j["volume"].is_number())
	{
		sapi_set_volume(j["volume"].as_integer());
	}
	if (j["disabled"].is_bool())
	{
		sa_disable = j["disabled"].as_bool();
	}
}
std::string sapi_get_profile()
{
	return sa_profile.dump();
}
void sapi_set_rate(int r)
{
	sa_pVoice->SetRate(r);
	sa_profile["rate"] = r;
}
void sapi_set_volume(int v)
{
	sa_pVoice->SetVolume(v);
	sa_profile["volume"] = v;
}
void delete_speak()
{
	if (sa_discard) return;
	sa_discard = true;
	sa_th_speak.join();
	sa_queue.clear();
	sa_pVoice->Release();
}
int sapi_disable()
{
	sa_disable = true;
	sa_profile["disabled"] = sa_disable;
	return 0;
}
int sapi_enable()
{
	sa_disable = false;
	sa_profile["disabled"] = sa_disable;
	return 0;
}

int sapi_init()
{
	create_speak();
	std::string pf;
	conf_get_sapi(pf);
	if (pf.length())
		sapi_set_profile(pf);

	e_server += [](event t, connection* c)
	{
		if (!c) return;
		if (t == event::join)
		{
			sapi_say(fmt::format("已加入服务器'{}'。", c->host));
		}
		else if (t == event::auth)
		{

		}
		else if (t == event::left)
		{
			sapi_say(fmt::format("已离开服务器'{}'。", c->host));
		}
	};
	e_channel += [](event t, channel* c)
	{
		if (!c) return;
		if (t == event::join)
		{
			sapi_say(fmt::format("已加入频道'{}'", c->name));
		}
		else if (t == event::left)
		{
			sapi_say(fmt::format("已离开频道'{}'", c->name));
		}
	};
	e_entity += [](event t, entity* e)
	{
		if (!e) return;
		if (t == event::join)
		{
			sapi_say(fmt::format("‘{}’已加入频道。", e->name));
		}
		else if (t == event::left)
		{
			sapi_say(fmt::format("‘{}’已离开频道。", e->name));
		}
	};

	return 0;
}
int sapi_uninit()
{
	delete_speak();
	return 0;
}
