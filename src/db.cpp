#include "db.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <functional>
#include <map>
#include <assert.h>
#include "sql.h"
#include "server.h"
#include "permissions.h"

static int db_version = -1;
static char* err_msg = nullptr;
int db_create_structure(sqlite3* db, char** msg)
{
	std::string sql;	sql =
		"CREATE TABLE IF NOT EXISTS meta(\
		id INT PRIMARY KEY NOT NULL,\
		version INT NOT NULL,\
		app_ver INT NOT NULL\
		);";
	int r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);
	assert(!r);
	_map kv;
	sql = "SELECT * FROM meta WHERE id = 0;";
	r = sqlite3_exec(db, sql.c_str(), &kv, &err_msg);
	assert(!r);
	if (kv.size() == 0)
	{
		//sql = "UPDATE meta SET version = 1, app_ver = 1 where id = 0;";
		sql = "INSERT INTO meta (id,version,app_ver) VALUES (0,0,1);";
		r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);
		assert(!r);
	}
	else
		db_version = atoi(kv["version"].c_str());
	kv.clear();

	sql =
		"CREATE TABLE IF NOT EXISTS users(\
		suid INT PRIMARY KEY NOT NULL,\
		uuid INT NOT NULL DEFAULT 0,\
		name TEXT NOT NULL,\
		avatar TEXT DEFAULT NULL,\
		token TEXT DEFAULT NULL,\
		state TEXT NOT NULL DEFAULT 'normal',\
		privilege INT NOT NULL DEFAULT 0\
		);";
	r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);

	assert(!r);

	sql =
		"CREATE TABLE IF NOT EXISTS channel(\
		chid INTEGER PRIMARY KEY AUTOINCREMENT,\
		parent INT NOT NULL DEFAULT 0,\
		owner INT NOT NULL DEFAULT 0,\
		name TEXT NOT NULL,\
		icon TEXT DEFAULT NULL,\
		desc TEXT DEFAULT 'This is a channel.',\
		capacity INT NOT NULL DEFAULT 16,\
		privilege TEXT NOT NULL DEFAULT ''\
		);";
	r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);
	assert(!r);

	sql = "SELECT COUNT(*) FROM channel;";
	r = sqlite3_exec(db, sql.c_str(), &kv, &err_msg);
	assert(!r);
	if (kv["COUNT(*)"] == "0")
	{
		//sql = "UPDATE meta SET version = 1, app_ver = 1 where id = 0;";
		sql = "INSERT INTO channel (chid,name) VALUES (1,'Default Channel');";
		r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);
		assert(!r);
	}
	kv.clear();

	db_upgrade(db);
	return 0;
}
int db_upgrade(sqlite3* db)
{
	std::string sql;
	if (db_version == 0)
	{
		sql =
			"\
			CREATE TABLE IF NOT EXISTS channel_role(\
			id INTEGER PRIMARY KEY AUTOINCREMENT,\
			name TEXT UNIQUE NOT NULL,\
			permission TEXT NOT NULL,\
			tag TEXT DEFAULT NULL,\
			level INT DEFAULT 0);\
			CREATE TABLE IF NOT EXISTS server_role(\
			id INTEGER PRIMARY KEY AUTOINCREMENT,\
			name TEXT UNIQUE NOT NULL,\
			permission TEXT NOT NULL,\
			tag TEXT DEFAULT NULL,\
			level INT DEFAULT 0);\
			CREATE TABLE IF NOT EXISTS role_key(\
			id INTEGER PRIMARY KEY AUTOINCREMENT,\
			gener INT NOT NULL,\
			code TEXT UNIQUE NOT NULL,\
			channel_id INT DEFAULT NULL,\
			channel_role TEXT DEFAULT NULL,\
			server_role TEXT DEFAULT NULL,\
			date TEXT NOT NULL DEFAULT (datetime('now')),\
			user TEXT DEFAULT NULL);\
			CREATE TABLE IF NOT EXISTS grant_log(\
			operator INT NOT NULL,\
			user INT NOT NULL,\
			from_sr TEXT,\
			from_cr TEXT,\
			to_sr TEXT,\
			to_cr TEXT,\
			chid INT DEFAULT NULL\
			);\
			INSERT INTO channel_role(name,permission,tag,level) VALUES ('Owner','channel.owner','owner',100);\
			INSERT INTO server_role (name,permission,tag,level) VALUES ('Owner','server.owner','owner',100);\
			INSERT INTO channel_role(name,permission,tag) VALUES ('Default','channel.default','default');\
			INSERT INTO server_role (name,permission,tag) VALUES ('Default','server.default','default');\
			ALTER TABLE channel ADD COLUMN default_role TEXT DEFAULT NULL;\
			ALTER TABLE users ADD COLUMN role TEXT DEFAULT NULL;\
			CREATE TABLE IF NOT EXISTS channel_user(\
			suid INT NOT NULL,\
			chid INT NOT NULL,\
			role TEXT NOT NULL,\
			mute INT NOT NULL DEFAULT 0,\
			silent INT NOT NULL DEFAULT 0,\
			PRIMARY KEY(suid, chid));\
			UPDATE meta set version = 1 where id = 0;\
			";
		db_version = 1;
		int r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);
		assert(!r);

	}
	return 0;
}
r instance::db_check_user(uint32_t& suid, std::string& token, std::string& msg)
{
	std::string sql = fmt::format("SELECT token,state FROM users WHERE suid = {} LIMIT 1;", suid);
	_map kv;
	auto hr = sqlite3_exec(db, sql.c_str(), &kv, db_msg);
	assert(!hr);
	if (kv.size() && token.length())
	{
		if ("normal" != kv["state"])
		{
			msg = kv["state"];
			return r::e_state;
		}
		if (token == kv["token"])
		{
			return r::ok;
		}
		msg = "invalid token";
		return r::e_auth;
	}
	else
	{
		if (kv.size())
			suid = randu32();
		token = token_gen();
		sql = fmt::format("INSERT INTO users (name,suid,token) VALUES ('NewUser',{},'{}');", suid, token);
		hr = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, db_msg);
		msg = "new user";
		assert(!hr);
		return r::new_user;
	}
}
//int instance::db_get_privilege(suid_t suid)
//{
//	auto sql = sqlite3_mprintf("SELECT * from users WHERE suid = %d LIMIT 1;", suid);
//	char* msg;
//	_map kv;
//	auto r = sqlite3_exec(db, sql, &kv, &msg);
//	assert(!r);
//	if (kv.count("privilege"))
//		return stru64(kv["privilege"]);
//	return 0;
//}
int instance::db_get_roles()
{
	server_roles.clear();
	channel_roles.clear();
	auto sql = sqlite3_mprintf("SELECT * from server_role;");
	char* msg;
	_cb_cmap kv;
	auto r = sqlite3_exec(db, sql, [this](_cmap kv)
		{
			pm_set s;
			command_buffer cb;
			if (kv["permission"])
			{
				cb.append(kv["permission"], strlen(kv["permission"]));
				cb.append("\n", 1);
			}
			else
			{
				cb.append(default_server_role.c_str(), default_server_role.length());
				cb.append("\n", 1);
			}
			command c;
			while (cb.parse(c) > 0)
			{
				if (c.n_args())
					s.permissions.insert(c[0]);
				c.clear();
			}
			if (s.permissions.count("server.owner"))
			{
				cb.append(owner_server_role.c_str(), owner_server_role.length());
				while (cb.parse(c) > 0)
				{
					if (c.n_args())
						s.permissions.insert(c[0]);
					c.clear();
				}
			}
			if (s.permissions.count("server.default"))
			{
				cb.append(default_server_role.c_str(), default_server_role.length());
				while (cb.parse(c) > 0)
				{
					if (c.n_args())
						s.permissions.insert(c[0]);
					c.clear();
				}
			}
			s.level = stru64(kv["level"]);
			server_roles[kv["name"]] = s;
		}, &msg);
	sqlite3_free(sql);
	sql = sqlite3_mprintf("SELECT * from channel_role;");
	assert(!r);
	r = sqlite3_exec(db, sql, [this](_cmap kv)
		{
			pm_set s;
			command_buffer cb;
			if (kv["permission"])
			{
				cb.append(kv["permission"], strlen(kv["permission"]));
				cb.append("\n", 1);
			}
			else
			{
				cb.append(default_server_role.c_str(), default_server_role.length());
				cb.append("\n", 1);
			}
			command c;
			while (cb.parse(c) > 0)
			{
				if (c.n_args())
					s.permissions.insert(c[0]);
				c.clear();
			}
			if (s.permissions.count("channel.default"))
			{
				cb.append(default_channel_role.c_str(), default_channel_role.length());
				while (cb.parse(c) > 0)
				{
					if (c.n_args())
						s.permissions.insert(c[0]);
					c.clear();
				}
			}
			if (s.permissions.count("channel.owner"))
			{
				cb.append(owner_channel_role.c_str(), owner_channel_role.length());
				while (cb.parse(c) > 0)
				{
					if (c.n_args())
						s.permissions.insert(c[0]);
					c.clear();
				}
			}
			s.level = stru64(kv["level"]);
			channel_roles[kv["name"]] = s;
		}, &msg);
	sqlite3_free(sql);
	assert(!r);
	return 0;

}
int instance::db_get_role_server(connection* c)
{
	if (!c || !c->suid) return -1;
	auto sql = sqlite3_mprintf("SELECT \
		IFNULL(\
			(SELECT role FROM users WHERE suid = %u),\
			(SELECT name FROM server_role WHERE tag = 'default'))\
		AS role; ", c->suid);
	char* msg;
	_map kv;
	auto r = sqlite3_exec(db, sql, &kv, &msg);
	sqlite3_free(sql);
	if (kv.count("role"))
	{
		c->role_server = kv["role"];
	}
	assert(!r);
	return 0;
}
int instance::db_get_role_channel(connection* c)
{
	if (!c || !c->suid) return -1;
	auto sql = sqlite3_mprintf("SELECT \
		IFNULL(\
			(SELECT role FROM channel_user WHERE suid = %u and chid = %u),\
			(IFNULL(\
				(SELECT default_role FROM channel WHERE chid = %u),\
				(SELECT name FROM channel_role WHERE tag = 'default'))\
			)\
		)AS role; ", c->suid, c->current_chid, c->current_chid);
	char* msg;
	_map kv;
	auto r = sqlite3_exec(db, sql, &kv, &msg);
	sqlite3_free(sql);
	c->role_channel = kv["role"];
	kv.clear();
	sql = sqlite3_mprintf("SELECT mute,silent FROM channel_user WHERE suid = %u and chid = %u;", c->suid, c->current_chid);
	r = sqlite3_exec(db, sql, &kv, &msg);
	if (kv.count("mute"))
	{
		c->state.man_mute = stru64(kv["mute"]);
		c->state.man_silent = stru64(kv["silent"]);
	}
	sqlite3_free(sql);
	assert(!r);
	return 0;
}
int instance::db_get_role_channel(suid_t suid, chid_t chid, std::string& role)
{
	if (!suid || !chid) return -1;
	auto sql = sqlite3_mprintf("SELECT \
		IFNULL(\
			(SELECT role FROM channel_user WHERE suid = %u and chid = %u),\
			(IFNULL(\
				(SELECT default_role FROM channel WHERE chid = %u),\
				(SELECT name FROM channel_role WHERE tag = 'default'))\
			)\
		)AS role; ", suid, chid, chid);
	char* msg;
	_map kv;
	auto r = sqlite3_exec(db, sql, &kv, &msg);
	sqlite3_free(sql);
	role = kv["role"];
	assert(!r);
	return 0;
}
int instance::db_gen_role_key(suid_t suid, std::string s_role, std::string c_role, chid_t chid, std::string& code)
{
	code = token_gen();
	auto sql = sqlite3_mprintf("INSERT INTO role_key (gener,code,channel_role,server_role,channel_id) VALUES \
		(%u, %Q,\
			(SELECT name FROM channel_role WHERE name = %Q),\
			(SELECT name FROM server_role WHERE name = %Q),\
			%u); ", suid, code.c_str(), c_role.c_str(), s_role.c_str(), chid);
	char* msg;
	_map kv;
	auto r = sqlite3_exec(db, sql, &kv, &msg);
	sqlite3_free(sql);
	if (r)
		code = "";
	assert(!r);
	return r;
}
int instance::db_gen_role_key_tag(suid_t suid, std::string s_tag, std::string c_tag, chid_t chid, std::string& code)
{
	code = token_gen();
	auto sql = sqlite3_mprintf("INSERT INTO role_key (gener,code,channel_role,server_role,channel_id) VALUES \
		(%u, %Q,\
			(SELECT name FROM channel_role WHERE tag = %Q),\
			(SELECT name FROM server_role WHERE tag = %Q),\
			%u); ", suid, code.c_str(), c_tag.c_str(), s_tag.c_str(), chid);
	char* msg;
	_map kv;
	auto r = sqlite3_exec(db, sql, &kv, &msg);
	sqlite3_free(sql);
	if (r)
		code = "";
	assert(!r);
	return r;

}
bool instance::db_user_exist(suid_t suid)
{
	auto sql = sqlite3_mprintf("SELECT COUNT(*) FROM users WHERE suid = %u;", suid);
	char* msg;
	_map kv;
	auto r = sqlite3_exec(db, sql, &kv, &msg);
	sqlite3_free(sql);
	int c = stru64(kv["COUNT(*)"]);
	assert(!r);
	return c;
}
int instance::db_grant(suid_t op, suid_t u, chid_t* cid, char* to_s, char* to_c)
{
	char* msg;
	char* sp_sf = nullptr;
	char* sp_cf = nullptr;
	char* sp_ci = nullptr;
	if (to_s)
	{
		sp_sf = sqlite3_mprintf("SELECT IFNULL(\
			(\
				SELECT role\
				FROM users\
				WHERE suid = %u\
				),\
			(\
				SELECT name\
				FROM server_role\
				WHERE tag = 'default'\
				)\
		)", u);
	}
	else
	{
		sp_sf = sqlite3_mprintf("NULL");
	}
	if (to_c)
	{
		sp_cf = sqlite3_mprintf("SELECT IFNULL(\
			(\
				SELECT role\
				FROM channel_user\
				WHERE suid = %u\
				and chid = %u\
				),\
			(\
				IFNULL(\
					(\
						SELECT default_role\
						FROM channel\
						WHERE chid = %u\
						),\
					(\
						SELECT name\
						FROM channel_role\
						WHERE tag = 'default'\
						)\
				)\
				)\
		)", u, *cid, *cid);
		sp_ci = sqlite3_mprintf("%u", *cid);
	}
	else
	{
		sp_cf = sqlite3_mprintf("NULL");
	}
	char* sql = sqlite3_mprintf("INSERT INTO grant_log (\
		operator,\
		user,\
		from_sr,\
		from_cr,\
		to_sr,\
		to_cr,\
		chid\
	)\
		VALUES(\
			%u,\
			%u,\
			(%z),\
			(%z),\
			(%Q),\
			(%Q),\
			%z\
		);", op, u, sp_sf, sp_cf, to_s, to_c, sp_ci);
	auto r = sqlite3_exec(db, sql, nullptr, nullptr, &msg);
	sqlite3_free(sql);
	if (to_s)
	{
		sql = sqlite3_mprintf("UPDATE users SET role = %Q WHERE suid = %u;", to_s, u);
		auto r = sqlite3_exec(db, sql, nullptr, nullptr, &msg);
		sqlite3_free(sql);
	}
	if (to_c)
	{
		sql = sqlite3_mprintf("INSERT OR REPLACE INTO channel_user (suid,chid,role) VALUES (%u,%u,%Q);", u, *cid, to_c);
		auto r = sqlite3_exec(db, sql, nullptr, nullptr, &msg);
		sqlite3_free(sql);
	}
	return 0;
}
int instance::db_delete_channel(chid_t chid)
{
	auto sql = sqlite3_mprintf("DELETE FROM channel WHERE chid = %u;DELETE FROM channel_user WHERE chid = %u;", chid, chid);
	char* msg;
	auto r = sqlite3_exec(db, sql, nullptr, nullptr, &msg);
	sqlite3_free(sql);
	assert(!r);
	return r;
}
int instance::db_get_channels(chid_t parent, std::vector<chid_t>& members)//
{
	members.clear();
	auto sql = sqlite3_mprintf("\
		with recursive child as (\
		select chid\
		from channel\
		where chid = %u\
		union all\
		select channel.chid\
		from child\
		join channel on child.chid = channel.parent\
		)\
		select *\
		from child;", parent);
	char* msg;
	auto r = sqlite3_exec(db, sql, [this, &members](_map& kv)
		{
			members.push_back(stru64(kv["chid"]));
		}, &msg);
	sqlite3_free(sql);
	assert(!r);
	return r;
}
int instance::db_allocate_chid()
{
	auto sql = "select chid from channel order by chid DESC limit 1;";
	char* msg;
	_map kv;
	auto r = sqlite3_exec(db, sql, &kv, &msg);
	assert(!r);
	return stru64(kv.begin()->second) + 1;
}
int instance::db_get_channel(channel* c)
{
	char* msg;
	auto sql = sqlite3_mprintf("SELECT * FROM channel WHERE chid = %u LIMIT 1;", c->chid);
	auto r = sqlite3_exec(
		db, sql, [this, c](_map& kv)
		{
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
		&msg);
	sqlite3_free(sql);
	assert(!r);
	return r;
}

int instance::db_create_channel(chid_t c, chid_t p, std::string name, std::string desc, chid_t owner)
{
	char* msg;
	auto sql = sqlite3_mprintf("INSERT INTO channel (chid,parent,name,desc,owner) values (%u,%u,%Q,%Q,%u);",
		c, p, name.c_str(), desc.c_str(), owner);
	auto r = sqlite3_exec(db, sql, nullptr, nullptr, &msg);
	sqlite3_free(sql);
	assert(!r);
	if (owner)
	{
		sql = sqlite3_mprintf("INSERT INTO channel_user (suid, chid, role) values (%u,%u,(SELECT name FROM channel_role WHERE tag = 'owner'));",
			owner, c);
		r = sqlite3_exec(db, sql, nullptr, nullptr, &msg);
		sqlite3_free(sql);
		assert(!r);
	}
	return r;
}
int instance::db_update_channel(chid_t c, const char* name, const char* desc)
{
	char* msg;
	auto sql = sqlite3_mprintf("\
UPDATE channel \
SET name = IFNULL(\
	%Q,\
	(\
		SELECT name \
		FROM channel \
		WHERE chid = %u\
		)\
),\
desc = IFNULL(\
	%Q,\
	(\
		SELECT desc \
		FROM channel \
		WHERE chid = %u\
		)\
) \
WHERE chid = %u;",
name, c, desc, c, c);
	auto r = sqlite3_exec(db, sql, nullptr, nullptr, &msg);
	sqlite3_free(sql);
	assert(!r);
	return r;

}