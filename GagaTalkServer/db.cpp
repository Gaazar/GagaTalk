#include "db.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <functional>
#include <map>
#include <assert.h>
#include "sql.h"
#include "server.h"

int db_create_structure(sqlite3* db, char** msg)
{
	int db_version = -1;
	std::string sql;	sql =
		"CREATE TABLE IF NOT EXISTS meta(\
		id INT PRIMARY KEY NOT NULL,\
		version INT NOT NULL,\
		app_ver INT NOT NULL\
		);";
	int r = sqlite3_exec(db, sql.c_str(), nullptr, 0, msg);
	assert(!r);
	_map kv;
	sql = "SELECT * FROM meta WHERE id = 0;";
	r = sqlite3_exec(db, sql.c_str(), &kv, msg);
	assert(!r);
	if (kv.size() == 0)
	{
		//sql = "UPDATE meta SET version = 1, app_ver = 1 where id = 0;";
		sql = "INSERT INTO meta (id,version,app_ver) VALUES (0,1,1);";
		r = sqlite3_exec(db, sql.c_str(), nullptr, 0, msg);
		assert(!r);
	}
	else
		db_version = atoi(kv["id"].c_str());
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
	r = sqlite3_exec(db, sql.c_str(), nullptr, 0, msg);
	assert(!r);

	sql =
		"CREATE TABLE IF NOT EXISTS channel(\
		chid INT PRIMARY KEY AUTOINCREMENT,\
		parent INT NOT NULL DEFAULT 0,\
		owner INT NOT NULL DEFAULT 0,\
		name TEXT NOT NULL,\
		icon TEXT DEFAULT NULL,\
		desc TEXT DEFAULT 'This is a channel.',\
		capacity INT NOT NULL DEFAULT 16,\
		privilege TEXT NOT NULL DEFAULT ''\
		);";
	r = sqlite3_exec(db, sql.c_str(), nullptr, 0, msg);
	assert(!r);

	sql = "SELECT COUNT(*) FROM channel;";
	r = sqlite3_exec(db, sql.c_str(), &kv, msg);
	assert(!r);
	if (kv["COUNT(*)"] == "0")
	{
		//sql = "UPDATE meta SET version = 1, app_ver = 1 where id = 0;";
		sql = "INSERT INTO channel (chid,name) VALUES (1,'Default Channel');";
		r = sqlite3_exec(db, sql.c_str(), nullptr, 0, msg);
		assert(!r);
	}
	kv.clear();

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
int instance::db_get_privilege(suid_t suid)
{
	auto sql = sqlite3_mprintf("SELETE * from users WHERE suid = %d LIMIT 1;", suid);
	char* msg;
	_map kv;
	auto r = sqlite3_exec(db, sql, &kv, &msg);
	assert(!r);
	if (kv.count("privilege"))
		return stru64(kv["privilege"]);
	return 0;
}
int instance::db_create_channel(ChannelDesc& cd)
{
	return 0;
}
int instance::db_delete_channel(chid_t id)
{
	return 0;
}
