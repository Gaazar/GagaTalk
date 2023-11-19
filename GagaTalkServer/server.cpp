#include "server.h"
#include "db.h"
#pragma comment(lib, "ws2_32.lib")
#include <stdio.h>
#include <sstream>
#ifdef _MSC_VER
#include "WS2tcpip.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable : 4996)
#endif
#ifdef __GNUC__
#define INVALID_SOCKET -1
#define closesocket(sk) shutdown(sk, SHUT_RDWR)
#endif

#define FMT_HEADER_ONLY
#include <fmt/core.h>
bool cmpsaddr(sockaddr_in* a, sockaddr_in* b)
{
#ifdef _MSC_VER
	// windows
	return a->sin_addr.S_un.S_addr == b->sin_addr.S_un.S_addr && a->sin_port == b->sin_port;
#endif
#ifdef __GNUC__
	// linux
	return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
#endif
}
bool terminated = false;
int instance::start(const char* dbn)
{
	last_clean = time(nullptr);
	auto hr = sqlite3_open(dbn, &db);
	hr = db_create_structure(db, db_msg);
	db_get_roles();
	std::string sql =
		"SELECT * FROM channel;";
	sqlite3_exec(
		db, sql.c_str(), [this](_map& kv)
		{
			channel* c = new channel();
			c->chid = std::strtoul(kv["chid"].c_str(), nullptr, 10);
			c->name = kv["name"];
			c->parent = std::strtoul(kv["parent"].c_str(), nullptr, 10);
			c->owner = std::strtoul(kv["owner"].c_str(), nullptr, 10);
			c->description = kv["desc"];
			c->capacity = std::strtoul(kv["capacity"].c_str(), nullptr, 10);
			c->privilege = kv["privilege"];
			c->session_id = randu32();
			c->inst = this;
			channels[c->chid] = c; },
		db_msg);
	sk_lsn = socket(AF_INET, SOCK_STREAM, 0);
	if (!sk_lsn)
	{
		printf(" create socket error: %s (%d)\n", strerror(errno), errno);
		return -1;
	}
	sk_voip = socket(AF_INET, SOCK_DGRAM, 0);
	if (!sk_voip)
	{
		printf(" create socket error: %s (%d)\n", strerror(errno), errno);
		return -1;
	}
	sockaddr_in saddr;
#ifdef _MSC_VER
	saddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#endif
#ifdef __GNUC__
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(7970);
	if (bind(sk_lsn, (sockaddr*)&saddr, sizeof(sockaddr)) != 0)
	{
		closesocket(sk_lsn);
		sk_lsn = 0;
		closesocket(sk_voip);
		sk_voip = 0;
		printf(" bind socket error: %s (%d)\n", strerror(errno), errno);
		return -3;
	}
#ifdef _MSC_VER
	saddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#endif
#ifdef __GNUC__
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(17970);
	if (bind(sk_voip, (sockaddr*)&saddr, sizeof(sockaddr)) != 0)
	{
		closesocket(sk_lsn);
		sk_lsn = 0;
		closesocket(sk_voip);
		sk_voip = 0;
		printf(" bind socket error: %s (%d)\n", strerror(errno), errno);
		return -3;
	}
	if (listen(sk_lsn, 16) == -1)
	{
		printf(" listen socket error: %s (%d)\n", strerror(errno), errno);
		return -1;
	}
	th_listen = std::thread([this]()
		{ listen_thread(); });
	th_voip = std::thread([this]()
		{ voip_recv_thread(); });
	return 0;
}
int instance::listen_thread()
{
	while (!discard && !terminated)
	{
		SOCKET sk_conn = 0;
		sockaddr_in caddr;
		socklen_t caddr_sz = sizeof(caddr);
		memset(&caddr, 0, sizeof(caddr));
		if ((sk_conn = accept(sk_lsn, (sockaddr*)&caddr, &caddr_sz)) == INVALID_SOCKET)
		{
			printf(" accept socket error: %s (errno :%d)\n", strerror(errno), errno);
			return -2;
		}
		else
		{
			std::lock_guard<std::mutex> g(m_conn);
			if (time(nullptr) > last_clean + 10)
			{
				for (auto i = conn_verifing.begin(); i != conn_verifing.end();)
				{
					if ((*i)->discard)
					{
						delete* i;
						i = conn_verifing.erase(i);
					}
					else
					{
						i++;
					}
				}
			}
			connection* conn = new connection();
			conn->sk_cmd = sk_conn;
			// conn->addr = caddr;
			memset(&conn->addr, 0, sizeof conn->addr);
			conn->server = this;
			conn->th_cmd = std::thread([this, conn]()
				{ conn->recv_cmd_thread(); });

			conn_verifing.push_back(conn);
			auto ip = inet_ntoa(caddr.sin_addr);
			printf("[I]: Connection[%d] established from %s:%d\n", sk_conn, ip, caddr.sin_port);
		}
	}
	return 0;
}
int instance::voip_recv_thread()
{
	char buffer[1536];
	while (!discard)
	{
		sockaddr_in from;
		socklen_t salen = sizeof(struct sockaddr);
		int len = recvfrom(sk_voip, buffer, 1536, 0, (sockaddr*)&from, &salen);
		if (len < 4)
			continue;
		if (*(uint32_t*)buffer != 0)
		{
			uint32_t ssid = *(uint32_t*)buffer;
			uint32_t suid = 0;
			for (auto& cli : connections)
			{
				if (cmpsaddr(&from, &cli.second->addr))
				{
					// if (*(uint32_t *)buffer != (cli->suid & 0xffffffff))
					//     return 0;
					if (!cli.second->state.man_mute)
						suid = cli.second->suid;
					break;
				}
			}
			if (suid)
			{
				for (auto& cn : channels)
				{

					if (cn.second->session_id == ssid)
					{
						*(uint32_t*)buffer = suid;
						cn.second->broadcast_voip_pak(buffer, len, suid);
						break;
					}
				}
			}
		}
		else if (len == 16)
		{
			if (*(uint32_t*)(buffer) != 0)
				continue;
			uint32_t suid = ((uint32_t*)buffer)[1];
			uint32_t ssid = ((uint32_t*)buffer)[2];
			uint32_t ccode = ((uint32_t*)buffer)[3];
			if (!connections.count(suid))
				continue;
			auto co = connections[suid];
			uint32_t chid = co->current_chid;
			if (!channels.count(chid))
				continue;
			auto cn = channels[chid];
			if (suid && cn->session_id == ssid && co->cert_code == ccode)
			{
				co->addr = from;
				printf("[I]: SUID:%u voip port = %d\n", suid, from.sin_port);
				auto bcmd = fmt::format("v {} {} {}\n", chid, ssid, 1);
				co->send_cmd(bcmd);
			}
		}
	}
	return 0;
}


void instance::broadcast(const char* buf, int sz, connection* ignore)
{
	for (auto& i : connections)
	{
		if (i.second != ignore)
		{
			i.second->send_buffer(buf, sz);
		}
	}
}
void instance::broadcast(std::string cmd, connection* ignore)
{
	broadcast(cmd.c_str(), cmd.length(), ignore);
}
void instance::send_voip(sockaddr_in* sa, const char* buf, int sz)
{
	auto r = sendto(sk_voip, buf, sz, 0, (sockaddr*)sa, sizeof(*sa));
}
void channel::broadcast_voip_pak(const char* buf, int sz, uint32_t ignore_suid)
{
	for (auto& i : clients)
	{
		if (i->suid == ignore_suid || i->state.man_silent)
			continue;
		inst->send_voip(&i->addr, buf, sz);
	}
}
void channel::broadcast_cmd(std::string cmd, connection* ignore)
{
	for (auto& i : clients)
	{
		if (i == ignore)
			continue;
		i->send_cmd(cmd);
	}

}
std::stringstream& channel::cgl_listinfo(std::stringstream& ss)
{
	ss << fmt::format("cd {} -n {} -d {} -p {}\n",
		chid, esc_quote(name), esc_quote(description), parent);
	return ss;
}

void instance::verified_connection(connection* c)
{
	for (auto i = conn_verifing.begin(); i != conn_verifing.end(); i++)
	{
		if (*i == c)
		{
			conn_verifing.erase(i);
			connections[c->suid] = c;
			break;
		}
	}
}
void instance::man_help(connection* conn)
{
	std::string s =
		"commands and usages:\n\
help : show avaliable commands and usage.\n\
mute <suid> [true|false|1|0] [-s:server range default is client range] : mute state. : mute and unmute a client.\n\
silent <suid> [true|false|1|0] [-s:server range default is client range] : silent state. : deaf a client.\n\
move <suid> <chid> : move a online client to the specified channel.\n\
dc <suid> [-m message]: disconnect a online client.\n\
newc <channel name> [-chid <specified id> -d <channel description> -c <capacity> -pid <parent id> -r <bitrate> -cc <1|2 :sound channel count>] : create a new channel\n\
modc <chid> [-n <new name> -d <new desc> -c <^> -cc <1|2> -r <bitrate> -pid <parent id>] : modify channel info.\n\
delc <chid> -sure -confirmed -delete_it: delete channel.\n\
grant <suid> [-c <role> -chid <chid>] [-s role] : grant permission\n\
";
	man_display(s, conn);
}
void instance::man_display(std::string s, connection* conn)
{
	if (conn)
	{
		conn->send_cmd(fmt::format("dp {}\n", esc_quote(s)));
	}
	else
		printf(s.c_str());
}
void instance::on_man_cmd(command& cmd, connection* conn)
{
	std::lock_guard<std::mutex> g(m_man);
	if (cmd[0] == "help")
	{
		man_help(conn);
	}
	else if (cmd[0] == "grk") mp_grk(cmd, conn);
	else if (cmd[0] == "grant") mp_grant(cmd, conn);
	else if (cmd[0] == "newc") mp_new_channel(cmd, conn);
	else if (cmd[0] == "modc") mp_mod_channel(cmd, conn);
	else if (cmd[0] == "delc") mp_del_channel(cmd, conn);
	else if (cmd[0] == "sql") mp_sql(cmd, conn);
	else if (cmd[0] == "mute") mp_mute(cmd, conn);
	else if (cmd[0] == "silent") mp_silent(cmd, conn);
}
void instance::cl_move(suid_t suid, chid_t chid)
{
	connections[suid]->join_channel(chid);
}
void instance::ch_delete(chid_t chid, chid_t to)
{
	if (channels.count(chid))
	{
		auto ch = channels[chid];
		ch->blocked = true;
		std::vector<connection*> conns = ch->clients;
		for (auto i : conns)
		{
			i->join_channel(to);
		}
		ch->session_id = 0;
		channels.erase(chid);
	}
}

uint64_t randu64()
{
	uint64_t v = 0;
	for (int i = 0; i < 8; i++)
	{
		v |= (rand() % 0xFF) << (i * 8);
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
std::string token_gen()
{
	std::stringstream ss;
	srand(time(nullptr));
	const char map[] = "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM";
	for (int i = 0; i < 32; i++)
	{
		ss << map[rand() % 52];
	}
	return ss.str();
}
