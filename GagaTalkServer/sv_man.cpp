#include "server.h"
#include "db.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>

void instance::mp_grk(command& cmd, connection* conn)
{
	std::string tk;
	std::stringstream ss;
	suid_t s = 0;
	if (conn) s = conn->suid;
	if (cmd.n_opts() == 0 && !conn)
	{
		db_gen_role_key_tag(0, "owner", "", 0, tk);
		man_display(tk + "\n", conn);
		return;
	}
	else
	{
		std::string c_r, s_r;
		chid_t c = 0;
		bool pass = true;
		if (conn)
		{
			auto& cr = server_roles[conn->role_server];
			if (!cr.permissions.count("role.keygen"))
			{
				ss << "permission denied. role.keygen\n";
				pass = false;
			}

		}
		if (pass && cmd.has_option("-c") && cmd.has_option("-chid"))
		{
			c_r = cmd["-c"];
			c = stru64(cmd["-chid"]);
			if (!channel_roles.count(c_r))
			{
				ss << "channel role: " << c_r << " does not exists.\n";
				pass = false;
			}
			if (!channels.count(c))
			{
				ss << "channel id: " << c << " does not exists.\n";
				pass = false;
			}
		}
		if (pass && cmd.has_option("-s"))
		{
			s_r = cmd["-s"];
			if (!server_roles.count(s_r))
			{
				ss << "server role: " << s_r << " does not exists.\n";
				pass = false;
			}
		}
		if (pass)
		{
			db_gen_role_key(s, s_r, c_r, c, tk);
			man_display(tk + "\n", conn);
		}
		else
		{
			man_display(ss.str(), conn);
		}
	}
}
void instance::mp_grant(command& cmd, connection* conn)
{
	std::stringstream ss;
	suid_t op = 0;
	suid_t u = 0;
	std::string to_s, * ts = nullptr;
	std::string to_c, * tc = nullptr;
	chid_t c = 0, * ac = nullptr;
	bool pass = true;
	if (cmd.n_args() < 2)
	{
		ss << "arguement mismatch.\n";
		man_display(ss.str(), conn);
		return;
	}
	u = stru64(cmd[1]);
	if (!db_user_exist(u))
	{
		ss << "user: " << u << " does not exists.\n";
		pass = false;
	}
	if (pass && cmd.has_option("-c"))
	{
		to_c = cmd["-c"];
		tc = &to_c;
		if (!channel_roles.count(to_c))
		{
			ss << "channel role: " << to_c << " does not exist\n";
			pass = false;
		}
		if (cmd.has_option("-chid"))
		{
			c = stru64(cmd["-chid"]);
			ac = &c;
		}
		if (!channels.count(c))
		{
			ss << "invalid chid: " << c << " \n";
			pass = false;
		}
	}
	if (pass && cmd.has_option("-s"))
	{
		to_s = cmd["-s"];
		ts = &to_s;
		if (!server_roles.count(to_s))
		{
			ss << "channel role: " << to_s << " does not exist\n";
			pass = false;
		}
	}
	if (pass && conn)
	{
		op = conn->suid;
		if (ts)
		{
			auto& cr = server_roles[conn->role_server];
			if (!cr.permissions.count("grant.server.role"))
			{
				ss << "permission denied. grant.server.role\n";
				pass = false;
			}
			if (pass && cr.level < server_roles[to_s].level)
			{
				ss << "permission denied: you can't grant a higer level server role to others.\n";
				pass = false;
			}
		}
		if (tc && pass)
		{
			std::string r;
			db_get_role_channel(op, c, r);
			auto& cr = channel_roles[r];
			auto& sr = server_roles[conn->role_server];
			if (!cr.permissions.count("grant.channel.role") && !sr.permissions.count("grant.channel.role"))
			{
				ss << "permission denied. grant.cahnnel.role\n";
				pass = false;
			}
			if (cr.level < channel_roles[to_c].level && !sr.permissions.count("grant.channel.role"))
			{
				ss << "permission denied: you can't grant a higer level channel role to others.\n";
				pass = false;
			}
		}
	}
	if (pass && (ts || tc))
	{
		db_grant(op, u, ac, ts ? (char*)ts->c_str() : nullptr, tc ? (char*)tc->c_str() : nullptr);
		if (connections.count(u))
		{
			auto uc = connections[u];
			if (ts)
			{
				uc->role_server = to_s;;
			}
			if (tc && uc->current_chid == c)
			{
				connections[u]->role_channel = to_c;;
			}
		}
	}
	else
	{
		if (!ts && !tc)
			ss << "option mismatch.\n";
		man_display(ss.str(), conn);
	}
}
void instance::mp_new_channel(command& cmd, connection* conn)
{
	std::stringstream ss;
	if (cmd.n_args() < 2)
	{
		ss << "arguement mismatch.\n";
		man_display(ss.str(), conn);
		return;
	}
	std::string& name = cmd[1];
	chid_t p = 0;
	if (cmd.n_opt_val("-p"))
	{
		p = stru64(cmd.option("-p"));
		if (!channels.count(p))
		{
			ss << "invalid parent: " << p << " does not exists.\n";
			man_display(ss.str(), conn);
			return;
		}
	}
	if (conn)
	{
		auto& sp = server_roles[conn->role_server];
		std::string cr;
		db_get_role_channel(conn->suid, p, cr);
		auto& cp = channel_roles[cr];
		if (!sp.permissions.count("create.channel") && !cp.permissions.count("create.channel"))
		{
			ss << "permission denied. create.channel\n";
			man_display(ss.str(), conn);
			return;
		}
	}
	chid_t ch = db_allocate_chid();
	std::string desc = "Default channel description.";
	if (cmd.n_opt_val("-chid"))
		ch = stru64(cmd.option("-chid"));
	if (cmd.n_opt_val("-d"))
		desc = cmd.option("-d");
	channel* c = new channel();
	db_create_channel(ch, p, name, desc, conn ? conn->suid : 0);
	c->chid = ch;
	db_get_channel(c);
	{
		std::lock_guard<std::mutex> g(m_channel);
		channels[ch] = c;
	}
	std::stringstream css;
	c->cgl_listinfo(css);
	broadcast(css.str());

}
void instance::mp_mod_channel(command& cmd, connection* conn)
{

}
void instance::mp_del_channel(command& cmd, connection* conn)
{
	std::stringstream ss;
	//delc <chid> -sure -confirmed -delete_it: delete channel.
	if (cmd.n_args() < 2)
	{
		ss << "arguement mismatch.\n";
		man_display(ss.str(), conn);
		return;
	}
	chid_t ch = stru64(cmd[1]);
	if (conn)
	{
		auto& sp = server_roles[conn->role_server];
		std::string cr;
		db_get_role_channel(conn->suid, ch, cr);
		auto& cp = channel_roles[cr];
		if (!sp.permissions.count("delete.channel") && !cp.permissions.count("delete.channel"))
		{
			ss << "permission denied. delete.channel\n";
			man_display(ss.str(), conn);
			return;
		}
	}
	if (!cmd.has_option("-sure") || !cmd.has_option("-confirmed") || !cmd.has_option("-delete_it"))
	{
		ss << "please confirm.\n";
		man_display(ss.str(), conn);
		return;
	}
	std::vector<chid_t> chs;
	db_get_channels(ch, chs);//query all child channels
	{
		//clear those channel, move clients inside them to default channel
		std::lock_guard<std::mutex> g(m_channel);
		std::stringstream css;
		for (auto& i : chs)
		{
			ch_delete(i);
			css << "cd " << i << " -r\n";
		}
		auto cmd = css.str();
		broadcast(cmd.c_str(), cmd.length());
	}
	//delete them from database
	for (auto& i : chs)
	{
		db_delete_channel(i);
	}

}
void instance::mp_sql(command& cmd, connection* conn)
{
	std::stringstream ss;
	//delc <chid> -sure -confirmed -delete_it: delete channel.
	if (cmd.n_args() < 2)
	{
		ss << "arguement mismatch.\n";
		man_display(ss.str(), conn);
		return;
	}
	std::string& sql = cmd[1];
	if (conn)
	{
		auto& sp = server_roles[conn->role_server];
		/*std::string cr;
		db_get_role_channel(conn->suid, ch, cr);
		auto& cp = channel_roles[cr];*/
		if (!sp.permissions.count("sql"))
		{
			ss << "permission denied. sql\n";
			man_display(ss.str(), conn);
			return;
		}
	}
	char* msg = nullptr;
	int itr = 0;
	auto r = sqlite3_exec(db, sql.c_str(), [&](_cmap& kv)
		{
			if (!itr)
			{
				for (auto i = kv.begin(); i != kv.end(); i++)
				{
					ss << i->first << "\t";
				}
				ss << "\n";
			}
			for (auto i = kv.begin(); i != kv.end(); i++)
			{
				if (i->second)
					ss << i->second << "\t";
				else
					ss << "NULL\t";
			}
			ss << "\n";

			itr++;
		}, &msg);
	if (msg)
	{
		ss << msg << "\n";
	}
	man_display(ss.str(), conn);
}
void instance::mp_mute(command& cmd, connection* conn)
{
	std::stringstream ss;
	if (cmd.n_args() < 2)
	{
		man_display("argument mismatch.\n", conn);
		return;
	}
	suid_t u = stru64(cmd[1]);
	if (!connections.count(u))
	{
		ss << "no such user suid:" << u << " \n";
		man_display(ss.str(), conn);
		return;
	}
	connection* cn = connections[u];
	chid_t ch = cn->current_chid;
	bool m = true;
	if (cmd.n_args() > 2)
	{
		m = stru64(cmd[2]);
	}
	if (conn)
	{
		auto& sp = server_roles[conn->role_server];
		std::string cr;
		db_get_role_channel(conn->suid, ch, cr);
		auto& cp = channel_roles[cr];
		if (!sp.permissions.count("admin.mute") && !cp.permissions.count("admin.mute"))
		{
			ss << "permission denied. admin.mute\n";
			man_display(ss.str(), conn);
			return;
		}
	}
	if (cmd.has_option("-s"))
		cn->state.man_mute = m;
	else
		cn->state.mute = m;
	std::stringstream css;
	css << "sc " << u;
	cn->state.cg_mute(css) << "\n";
	channels[ch]->broadcast_cmd(css.str());

}
void instance::mp_silent(command& cmd, connection* conn)
{
	std::stringstream ss;
	if (cmd.n_args() < 2)
	{
		man_display("argument mismatch.\n", conn);
		return;
	}
	suid_t u = stru64(cmd[1]);
	if (!connections.count(u))
	{
		ss << "no such user suid:" << u << " \n";
		man_display(ss.str(), conn);
		return;
	}
	connection* cn = connections[u];
	chid_t ch = cn->current_chid;
	bool m = true;
	if (cmd.n_args() > 2)
	{
		m = stru64(cmd[2]);
	}
	if (conn)
	{
		auto& sp = server_roles[conn->role_server];
		std::string cr;
		db_get_role_channel(conn->suid, ch, cr);
		auto& cp = channel_roles[cr];
		if (!sp.permissions.count("admin.silent") && !cp.permissions.count("admin.silent"))
		{
			ss << "permission denied. admin.silent\n";
			man_display(ss.str(), conn);
			return;
		}
	}
	if (cmd.has_option("-s"))
		cn->state.man_silent = m;
	else
		cn->state.silent = m;
	std::stringstream css;
	css << "sc " << u;
	cn->state.cg_silent(css) << "\n";
	channels[ch]->broadcast_cmd(css.str());
}