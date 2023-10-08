#include "client.h"
#include "gt_defs.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <assert.h>
#include <limits>
#include "native_util.hpp"
#include <configor/json.hpp>
#include "speak.h"
#include <windows.h>
#include <filesystem>
using namespace configor;
void connection::on_recv_cmd(command& cmd)
{
	if (status == state::verifing)
	{
		if (cmd[0] == "ch")
		{
			suid = std::strtoull(cmd[1].c_str(), nullptr, 10);
			auto v = cmd[2];
			if (v == "ok" || v == "new")
			{
				status = state::established;
				if (cmd.has_option("-t"))
				{
					conf_set_token(host, suid, cmd["-t"]);
				}
				sapi_join_server(host);
			}
			else
			{
				status = state::disconnect;
				disconnect();
			}
		}
	}
	else
	{
		if (cmd[0] != "pong")
			printf("[%s]\n", cmd.str().c_str());
		if (cmd[0] == "cd")
		{
			uint32_t chid = stru64(cmd[1]);
			if (!channels.count(chid))
			{
				channels[chid] = new channel();
			}
			channel* d = channels[chid];
			d->chid = chid;
			d->conn = this;
			if (cmd.has_option("-n"))
				d->name = cmd["-n"];
			if (cmd.has_option("-d"))
				d->description = cmd["-d"];
			if (cmd.has_option("-p"))
				d->parent = stru64(cmd["-p"]);
		}
		else if (cmd[0] == "rd")
		{
			uint32_t suid = stru64(cmd[1]);
			bool new_entity = false;
			if (!entities.count(suid))
			{
				if (cmd.has_option("-l"))
					return;
				auto ne = new entity();
				entities[suid] = ne;
				ne->suid = suid;
				ne->conn = this;
				new_entity = true;
			}
			entity* e = entities[suid];
			if (cmd.has_option("-l"))
			{
				entities.erase(suid);
				auto c = e->current_chid;
				channels[c]->erase(e);
				if (e->current_chid == chid)
					sapi_left_channel(e->name);
				delete e;
			}
			else
			{
				auto last_chid = e->current_chid;
				if (cmd.has_option("-n"))
					e->name = cmd["-n"];
				if (cmd.has_option("-u"))
					e->uuid = stru64(cmd["-u"]);
				if (cmd.has_option("-a"))
					e->avatar = cmd["-a"];
				if (cmd.has_option("-c"))
					e->current_chid = stru64(cmd["-c"]);
				if (new_entity)
				{
					if (e->current_chid == chid && mic)
						sapi_join_channel(e->name);
					channels[e->current_chid]->join(e, true);
					if (suid == this->suid)
					{
						current = channels[e->current_chid];
						chid = e->current_chid;
						channel_switch(0, e->current_chid);
					}

				}
				else if (last_chid != e->current_chid)
				{
					if (suid == this->suid)
					{
						current = channels[e->current_chid];
						channel_switch(last_chid, e->current_chid);
					}
					channels[last_chid]->erase(e);
					channels[e->current_chid]->join(e);
					if (suid != this->suid)
					{
						if (e->current_chid == chid)
						{
							sapi_join_channel(e->name);
						}
					}

				}
			}

		}
		else if (cmd[0] == "s")
		{
			uint32_t chid = stru64(cmd[1]);
			assert(current->chid == chid);
			ssid = stru64(cmd[2]);
			cert = stru64(cmd[3]);
			uint32_t buf[4];
			buf[0] = 0;
			buf[1] = suid;
			buf[2] = ssid;
			buf[3] = cert;
			send_voip_pack((const char*)buf, sizeof buf);
		}
		else if (cmd[0] == "v")
		{
			uint32_t chid = stru64(cmd[1]);
			assert(current->chid == chid);
			uint32_t ssid = stru64(cmd[2]);
			assert(this->ssid == ssid);
			uint32_t state = 0;
			state = stru64(cmd[3]);
			if (state == 1)
			{
				mic = plat_create_vr();
				mic->callback = [=](AudioFrame* f)
				{
					on_mic_pack(f);
				};
			}
			else
			{
				if (mic)
					plat_delete_vr(mic);
				mic = nullptr;
			}
		}
	}
}

void connection::on_mic_pack(AudioFrame* f)
{
	unsigned char buf[1480];
	((uint32_t*)buf)[0] = ssid;
	int len = opus_encode_float(aud_enc, f->samples, f->nSamples, &buf[4], 1480 - 4);
	if (len > 1)
	{
		send_voip_pack((const char*)buf, len + 4);
	}
}
void connection::join_channel(uint32_t chid)
{
	if (mic)
		plat_delete_vr(mic);
	mic = nullptr;
	auto cmd = fmt::format("j {}\n", chid);
	send_command(cmd.c_str(), cmd.length());
}

void connection::channel_switch(uint32_t from, uint32_t to)
{
	printf("channel siwtched from %d to %d\n", from, to);
	if (channels.count(from))
	{
		auto c = channels[from];
		for (auto i : c->entities)
		{
			i->remove_playback();
		}
	}
	if (channels.count(to))
	{
		current = channels[to];
		sapi_say(fmt::format("已加入频道'{}'", current->name));
		for (auto i : current->entities)
		{
			if (i->suid != suid)
				i->create_playback();
		}
	}
}

void channel::erase(entity* e)
{
	bool op = false;
	for (auto i = entities.begin(); i != entities.end(); i++)
	{
		if (*i == e)
		{
			entities.erase(i);
			op = true;
			break;
		}
	}
	if (op && chid == conn->current->chid)
	{
		e->remove_playback();
	}
}
void channel::erase(uint32_t suid)
{
	entity* e = nullptr;
	for (auto i = entities.begin(); i != entities.end(); i++)
	{
		if ((*i)->suid == suid)
		{
			e = *i;
			entities.erase(i);
			break;
		}
	}
	if (e && chid == conn->current->chid)
	{
		sapi_say(fmt::format("'{}'已离开频道", e->name));
		e->remove_playback();
	}

}

void channel::join(entity* e, bool is_new)
{
	for (auto i = entities.begin(); i != entities.end(); i++)
	{
		if ((*i) == e)
		{
			return;
		}
	}
	entities.push_back(e);
	if (this == conn->current && e->suid != conn->suid)
	{
		e->create_playback();
	}
}
void connection::set_client_volume(uint32_t suid, float vol, bool save)
{
	if (save)
		conf_set_uc_volume(host, suid, vol);
	if (!entities.count(suid)) return;
	entities[suid]->set_volume(vol);
}
float connection::get_client_volume(uint32_t suid)
{
	if (entities.count(suid))
		return entities[suid]->get_volume();
	return 0.0;
}
void connection::set_client_mute(uint32_t suid, bool mute, bool save)
{
	if (save)
		conf_set_uc_mute(host, suid, mute);
	if (!entities.count(suid)) return;
	entities[suid]->set_mute(mute);
}
bool connection::get_client_mute(uint32_t suid)
{
	if (entities.count(suid))
		return entities[suid]->get_mute();
	return false;
}

int client_init()
{
	platform_init();
	std::string str;
	if (conf_get_input_device(str))
	{
		plat_set_input_device(str);
	}
	if (conf_get_output_device(str))
	{
		plat_set_output_device(str);
	}
	float f;
	bool b;
	conf_get_global_volume(f);
	plat_set_global_volume(f);
	conf_get_global_mute(b);
	plat_set_global_mute(b);
	conf_get_global_silent(b);
	plat_set_global_silent(b);
	conf_get_sapi(str);

	sapi_init();

	str = sapi_get_profile();
	return 0;
}

void entity::load_profile()
{
	float v;
	bool b;
	conf_get_uc_volume(conn->host, suid, v);
	set_volume(v);
	conf_get_uc_mute(conn->host, suid, b);
	set_mute(b);
}
using namespace native;
download_pool c_dp;
void client_check_update()
{
	namespace fs = std::filesystem;
	printf("正在检查更新...");
	auto t = download_task("https://gaazar.cc/gagatalk/version.json", [](download_pool::task_status s, download_task* t)
		{
			if (s == download_pool::task_status::finished)
			{
				std::string sutf8(t->data, t->data + t->size);
				//auto ws = sutil::s2w(sutf8, CP_UTF8);
				json j = json::parse(sutf8);
				auto lv = j["latest"].as_integer();
				auto url = j["download"].as_string();
				printf("OK\n");
				if (lv > BUILD_SEQ)
				{
					auto dt = download_task(url, "Update.pak",
						[](download_pool::task_status s, download_task* t)
						{
							printf("Update package download status:%d\n", s);
							if (s == download_pool::task_status::finished)
							{
								client_uninit();
								SHELLEXECUTEINFO ShExecInfo = { 0 };
								ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
								ShExecInfo.fMask = SEE_MASK_DEFAULT;
								ShExecInfo.hwnd = NULL;
								ShExecInfo.lpVerb = L"runas";
								ShExecInfo.lpFile = L"Update.exe";
								ShExecInfo.lpParameters = L"";
								ShExecInfo.lpDirectory = NULL;
								ShExecInfo.nShow = SW_SHOW;
								ShExecInfo.hInstApp = NULL;
								ShellExecuteEx(&ShExecInfo);
								exit(80);
							}
						});
					if (j["log"].is_string())
					{
						printf("更新日志:\n%s\n", sutil::w2s(sutil::s2w(j["log"].as_string(), CP_UTF8)).c_str());
					}
					printf("正在下载更新...\n");
					c_dp.join_task(&dt);
				}
				else
					printf("已经是最新版本。\n");
				if (fs::exists("./temp/Update.exe"))
				{
					std::this_thread::sleep_for(std::chrono::seconds(3));
					std::error_code ec;
					fs::remove("./Update.exe", ec);
					if (ec) printf("UpdateError:%s at remove raw\n", ec.message().c_str());
					ec.clear();
					fs::copy("./temp/Update.exe", "./Update.exe", ec);
					if (ec) printf("UpdateError:%s at copy \n", ec.message().c_str());
					ec.clear();
					fs::remove("./temp/Update.exe", ec);
					if (ec) printf("UpdateError:%s at remove new\n", ec.message().c_str());
				}
			}
			else
				printf("\n无法检测更新信息。\n");
		});
	c_dp.join_task(&t);
}

int client_uninit()
{
	terminated = true;
	platform_uninit();
	sapi_uninit();
	return 0;
}
