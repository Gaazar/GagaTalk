#include "server.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#ifdef _MSC_VER

#endif
#ifdef __GNUC__
#define INVALID_SOCKET -1
//#define closesocket(sk) shutdown(sk, SHUT_RDWR);close(sk)
#define closesocket(sk) close(sk)
#endif
connection::connection()
{
}
void connection::release()
{
	if (discard)
		return;
	discard = true;
	release_debug();
	if (suid)
	{
		join_channel(0);
		server->connections.erase(suid);
		//server->conn_verifing.push_back(this);
	}
	suid = 0;
	uuid = 0;
	if (sk_cmd)
		closesocket(sk_cmd);
	sk_cmd = 0;
	activated = 0;
	cert_code = 0;
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
void connection::release_debug()
{
	if (debug.act == suid)
	{
		if (server->connections.count(debug.pas))
		{
			server->connections[debug.pas]->debug.act = 0;
			server->connections[debug.pas]->debug.pas = 0;
			server->connections[debug.pas]->send_cmd(fmt::format("rdc close {}\n", debug.act));
		}
	}
	else
	{
		if (server->connections.count(debug.act))
		{
			server->connections[debug.act]->debug.act = 0;
			server->connections[debug.act]->debug.pas = 0;
		}
		send_cmd(fmt::format("rdc close {}\n", debug.act));
	}
	debug.act = 0;
	debug.pas = 0;
}
void connection::on_recv_cmd(command& cmd)
{
	if (!server) return;
	std::string sql = "";
	if (cmd[0] == "ping")
	{
		send_cmd("pong\n");
	}
	else
		printf("[C:%lld][%s]\n", sk_cmd, cmd.str().c_str());
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
			res = fmt::format("ch {} {} -n {} -t {} -m {} -v {} -sn {} -sd {}\n", suid, s, name, token, escape(msg), VERSION_SEQ,
				escape(server->name), escape(server->description));
		else
			res = fmt::format("ch {} {} -n {} -m {} -v {} -sn {} -sd {}\n", suid, s, name, escape(msg), VERSION_SEQ,
				escape(server->name), escape(server->description));
		send_cmd(res);
		if (r == r::new_user || r == r::ok)
		{
			if (server->connections.count(suid))
			{
				server->connections[suid]->release();
			}
			cert_code = randu32();
			server->verified_connection(this);
			server->db_get_role_server(this);
			send_channel_list();//cd
			send_clients_list();//rd
			send_role_permissions();//role
			send_cmd("info\n");
			{
				//std::lock_guard<std::mutex> g(server->m_conn);
				//join_channel(1);

			}//auto bcmd = fmt::format("rd {} -n {} -c {}\n", suid, esc_quote(name), current_chid);
			//server->broadcast(bcmd.c_str(), bcmd.length(), this);
			//bcmd = fmt::format("s {} {} {}\n", current_chid, server->channels[current_chid]->session_id, cert_code);
			//send_cmd(bcmd);
		}
	}
	if (cert_code == 0) return;
	if (cmd[0] == "j")
	{
		if (cmd.n_args() < 2)
			return;
		auto chid = stru64(cmd[1]);
		std::lock_guard<std::mutex> g(server->m_conn);
		if (chid == 0)
		{
			join_channel(0);
			return;
		}
		if (!server->channels.count(chid))
			return;
		if (!permission("join", chid))
		{
			send_cmd(fmt::format("rej j -m 'permission denied: join'\n"));
			return;
		}
		if (cmd.has_option("-mon"))
		{
			if (!permission("monitor", chid))
			{
				send_cmd(fmt::format("rej j -m 'permission denied: monitor'\n"));
				return;
			}

			return;
		}
		join_channel(chid);
		//auto bcmd = fmt::format("rd {} -c {}\n", suid, chid);
		//server->broadcast(bcmd.c_str(), bcmd.length());
		//bcmd = fmt::format("s {} {} {}\n", current_chid, server->channels[current_chid]->session_id, cert_code);
		//send_cmd(bcmd);
	}
	else if (cmd[0] == "man")
	{
		cmd.remove_head();
		server->on_man_cmd(cmd, this);
	}
	else if (cmd[0] == "sc")
	{
		std::stringstream ss;
		int narg = 0;
		ss << "sc " << suid;
		if (cmd.n_opt_val("-mute"))
		{
			narg++;
			state.mute = stru64(cmd.option("-mute"));
			state.cg_mute(ss);
		}
		if (cmd.n_opt_val("-silent"))
		{
			narg++;
			state.silent = stru64(cmd.option("-silent"));
			state.cg_silent(ss);
		}
		if (narg)
		{
			ss << "\n";
			if (current_chid)
				server->channels[current_chid]->broadcast_cmd(ss.str(), this);
		}
	}
	else if (cmd[0] == "rdc")
	{
		if (!permission("remote.command", current_chid))
		{
			send_cmd(fmt::format("rej rc -m 'permission denied: remote.command'\n"));
			return;
		}
		if (cmd.n_args() < 3)
		{
			send_cmd(fmt::format("rej rc -m 'arg mism'\n"));
			return;
		}
		suid_t s = stru64(cmd[2]);
		if (cmd[1] == "open")
		{
			if (debug.act || debug.pas)
			{
				send_cmd(fmt::format("rej rc -m 'already in remote'\n"));
				return;
			}
			if (!server->connections.count(s))
			{
				send_cmd(fmt::format("rej rc -m 'user not exist'\n"));
				return;
			}
			if (s == suid)
			{
				send_cmd(fmt::format("rej rc -m 'yourself'\n"));
				return;
			}
			connection* pas = server->connections[s];
			debug.act = suid;
			debug.pas = s;
			pas->debug = debug;
			pas->send_cmd(fmt::format("rdc open {}\n", suid));
		}
		else if (cmd[1] == "close")
		{
			release_debug();
		}
	}
	else if (cmd[0] == "rdd")
	{
		if (!debug.pas || !debug.act)
			return;
		suid_t t = ((debug.pas == suid) ? debug.act : debug.pas);
		if (!server->connections.count(t))
		{
			release_debug();
			return;
		}
		if (t == debug.act)
			server->connections[t]->send_cmd(fmt::format("dp {}\n", escape(cmd.token(1))));
		else
			server->connections[t]->send_cmd(cmd.str() + "\n");
	}
}
int connection::send_cmd(std::string s)
{
	if (discard) return 0;
	return send(sk_cmd, s.c_str(), s.length(), 0);
}
int connection::send_buffer(const char* buf, int sz)
{
	if (discard) return 0;
	return send(sk_cmd, buf, sz, 0);
}
void connection::send_channel_list()
{
	std::stringstream ss;
	for (auto& kv : server->channels)
	{
		auto i = kv.second;
		//ss << fmt::format("cd {} -n {} -d {} -p {}\n",
		//	i->chid, esc_quote(i->name), esc_quote(i->description), i->parent);
		i->cgl_listinfo(ss);
	}
	send_cmd(ss.str());
}
void connection::send_clients_list()
{
	std::stringstream ss;
	for (auto& kv : server->connections)
	{
		auto i = kv.second;
		if (i->suid != suid && i->current_chid)
			ss << fmt::format("rd {} -n {} -c {}\n",
				i->suid, esc_quote(i->name), i->current_chid);
	}
	send_cmd(ss.str());
}
void connection::send_role_permissions()
{
	chid_t chid = current_chid;
	std::stringstream ss;
	ss << fmt::format("role {} ", role_server);
	if (chid)
	{
		ss << fmt::format("-chid {} -crole {} ", chid, role_channel);
	}
	ss << "\nperm ";
	for (auto& i : server->server_roles[role_server].permissions)
	{
		ss << i << " ";
	}
	if (chid)
	{
		for (auto& i : server->channel_roles[role_channel].permissions)
		{
			ss << i << " ";
		}
	}
	ss << "\n";
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
	srand(time(nullptr));
	while (!discard && !server->discard)
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
			printf("[I]: Connection[%lld] closed.\n", sk_cmd);
			release();
			return 1;
		}
	}
	return 0;
}
void connection::join_channel(uint32_t chid, bool mon)
{
	std::string cmd;
	chid_t befor = 0;
	monitoring = mon;
	if (current_chid != chid)
	{
		if (server->channels.count(current_chid))
		{
			befor = current_chid;
			auto cn = server->channels[current_chid];
			for (auto i = cn->clients.begin(); i != cn->clients.end(); i++)
			{
				if ((*i)->suid == this->suid)
				{
					cn->clients.erase(i);
					cmd = fmt::format("v {} {} 0\n", current_chid, cn->session_id);
					send_cmd(cmd);
					break;
				}
			}
		}
		if (chid == 0)
		{
			current_chid = 0;
			server->broadcast(fmt::format("rd {} -l\n", suid));
			return;
		}
		cert_code = randu32();
		if (server->channels.count(chid))
		{
			current_chid = chid;
			auto cn = server->channels[chid];
			if (cn->blocked)
			{
				return;
			}
			server->db_get_role_channel(this);
			cn->clients.push_back(this);
			std::stringstream ss;
			if (befor)
			{
				ss << fmt::format("rd {} -c {}\n", suid, chid);
				server->broadcast(ss.str());

			}
			else
			{
				//new client
				ss << fmt::format("rd {} -n {} -c {}\n", suid, esc_quote(name), chid);
				server->broadcast(ss.str());
			}
			ss.str("");
			ss.clear();
			ss << "sc " << suid;
			cg_state(ss) << "\n";
			cn->broadcast_cmd(ss.str());
			send_role_permissions();
			send_cmd(fmt::format("\ns {} {} {}\n", chid, cn->session_id, cert_code));
			send_clients_info();
		}
	}
}
//void connection::send_channel_info()
//{
//
//}
void connection::send_clients_info()
{
	std::stringstream ss;
	auto& ch = server->channels[current_chid];
	for (auto i : ch->clients)
	{
		if (i->suid != suid)
		{
			ss << fmt::format("sc {}",
				i->suid);
			i->cg_state(ss) << "\n";
		}
	}
	send_cmd(ss.str());

}
std::stringstream& connection::cg_state(std::stringstream& ss)
{
	ss << fmt::format(" -mute {} -silent {}",
		((int)state.man_mute) | (((int)state.mute) << 1),
		((int)state.man_silent) | (((int)state.silent) << 1));
	return ss;
}

std::stringstream& connection::debug_output(std::stringstream& ss)
{
	char ipaddr[128] = { 0 };
	debug_address(ipaddr, sizeof ipaddr);
	ss <<
		"DEBUG INFO:\n" <<
		"address:\t" << ipaddr << "\n" <<
		"port:\t" << stts.cmd_port << "\n" <<
		"suid:\t" << suid << "\n" <<
		"name:\t" << name << "\n" <<
		"channel:\t" << current_chid << "\n" <<
		"channel ssid:\t" << (current_chid ? server->channels[current_chid]->session_id : 0) << "\n" <<
		"cert:\t" << cert_code << "\n" <<
		"voice:\n" <<
		"\tport:\t" << addr.sin_port << "\n" <<
		"\ttx:" << stts.vo_tx_np << "\t" << stts.vo_tx_nb << "\n\trx:" << stts.vo_rx_np << "\t" << stts.vo_rx_nb << "\n" <<
		"\tlast ssid:\t" << stts.last_ssid << "\n"
		;
	ss << "remote debug:" <<
		"\tact:" << debug.act << "\tpas:" << debug.pas << "\n";
	return ss;
}