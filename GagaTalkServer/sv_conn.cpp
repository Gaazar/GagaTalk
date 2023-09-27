#include "server.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#ifdef _MSC_VER

#endif
#ifdef __GNUC__
#define INVALID_SOCKET -1
#define closesocket(sk) shutdown(sk, SHUT_RDWR)
#endif
connection::connection()
{
}
void connection::release()
{
	if (discard)
		return;
	discard = true;
	if (suid)
	{
		auto c = fmt::format("rd {} -l\n", suid);
		server->broadcast(c.c_str(), c.length(), this);
		join_channel(0);
		server->connections.erase(suid);
		server->conn_verifing.push_back(this);
	}
	suid = 0;
	uuid = 0;
	if (sk_cmd)
		closesocket(sk_cmd);
	sk_cmd = 0;
	activated = 0;
	cert_code = randu64();
#ifdef _MSC_VER
	addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#endif
#ifdef __GNUC__
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	addr.sin_port = 0;
	server = nullptr;
	th_cmd.detach();
}
void connection::on_recv_cmd(command& cmd)
{
	if (!server) return;
	std::string sql = "";
	//if (cmd[0] != "ping")
	printf("[C:%d][%s]\n", sk_cmd, cmd.str().c_str());
	if (cmd[0] == "hs")
	{
		if (cmd.n_args() < 3)
			return;
		suid = stru64(cmd[1]);
		name = cmd[2];
		std::string token, msg;
		if (cmd.n_args() > 3)
		{
			token = cmd[3];
		}
		if (cmd.has_option("-u"))
			uuid = stru64(cmd["-u"]);
		auto r = server->db_check_user(suid, token, msg);
		std::string s = "err";
		switch (r)
		{
		case r::ok:
			s = "ok";
			break;
		case r::new_user:
			s = "new";
			break;
		case r::e_auth:
			s = "auth";
			break;
		case r::e_state:
			s = "ban";
			break;
		default:
			break;
		}
		std::string res;
		if (r == r::new_user)
			res = fmt::format("ch {} {} -t {} -m {}\n", suid, s, token, escape(msg));
		else
			res = fmt::format("ch {} {} -m {}\n", suid, s, escape(msg));
		send_cmd(res);
		if (r == r::new_user || r == r::ok)
		{
			if (server->connections.count(suid))
			{
				server->connections[suid]->release();
			}
			cert_code = randu32();
			server->verified_connection(this);
			join_channel(1);
			send_channel_info();
			auto bcmd = fmt::format("rd {} -n {} -c {}\n", suid, esc_quote(name), current_chid);
			server->broadcast(bcmd.c_str(), bcmd.length(), this);
			send_clients_info();
			bcmd = fmt::format("s {} {} {}\n", current_chid, server->channels[current_chid]->session_id, cert_code);
			send_cmd(bcmd);
		}
	}
	else if (cmd[0] == "j")
	{
		if (cmd.n_args() < 2)
			return;
		auto chid = stru64(cmd[1]);
		if (!server->channels.count(chid))
			return;
		cert_code = randu32();
		join_channel(chid);
		auto bcmd = fmt::format("rd {} -c {}\n", suid, chid);
		server->broadcast(bcmd.c_str(), bcmd.length());
		bcmd = fmt::format("s {} {} {}\n", current_chid, server->channels[current_chid]->session_id, cert_code);
		send_cmd(bcmd);
	}
}
int connection::send_cmd(std::string s)
{
	return send(sk_cmd, s.c_str(), s.length(), 0);
}
int connection::send_buffer(const char* buf, int sz)
{
	return send(sk_cmd, buf, sz, 0);
}
void connection::send_channel_info()
{
	std::stringstream ss;
	for (auto& kv : server->channels)
	{
		auto i = kv.second;
		ss << fmt::format("cd {} -n {} -d {} -p {}\n",
			i->chid, esc_quote(i->name), esc_quote(i->description), i->parent);
	}
	send_cmd(ss.str());
}
void connection::send_clients_info()
{
	std::stringstream ss;
	for (auto& kv : server->connections)
	{
		auto i = kv.second;
		ss << fmt::format("rd {} -n {} -c {}\n",
			i->suid, esc_quote(i->name), i->current_chid);
	}
	send_cmd(ss.str());
}
int connection::recv_cmd_thread()
{
	// linux
	/*
	timeval tv_out;
	tv_out.tv_sec = 10;
	tv_out.tv_usec = 0;
	*/
	// windows
	int tv_out = 10000;
	auto hr = setsockopt(sk_cmd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv_out, sizeof(tv_out));
	char buffer[1024];
	int len = 0;
	command_buffer cmdb;
	command cmd;
	srand((int)this);
	while (!discard && !server->terminated)
	{
		len = recv(sk_cmd, buffer, sizeof buffer, 0);
		if (len > 0)
		{
			if (!activated)
			{
				// tv_out.tv_sec = 0;
				tv_out = 0;
				hr = setsockopt(sk_cmd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv_out, sizeof(tv_out));
			}
			activated++;
			int n_cmd;
			if (n_cmd = cmdb.append(buffer, len))
			{
				for (int i = 0; i < n_cmd; i++)
				{
					int c = cmdb.parse(cmd);
					on_recv_cmd(cmd);
					cmd.clear();
				}
			}
		}
		else
		{
			// std::lock_guard<std::mutex> g(server->m_conn);
			printf("[I]: Connection[%d] closed.\n", sk_cmd);
			release();
			return 1;
		}
	}
	return 0;
}
void connection::join_channel(uint32_t chid)
{
	if (current_chid != chid)
	{
		if (server->channels.count(current_chid))
		{
			auto cn = server->channels[current_chid];
			for (auto i = cn->clients.begin(); i != cn->clients.end(); i++)
			{
				if ((*i)->suid == this->suid)
				{
					cn->clients.erase(i);
					break;
				}
			}
		}
		if (server->channels.count(chid))
		{
			auto cn = server->channels[chid];
			cn->clients.push_back(this);
			current_chid = chid;
		}
	}
}