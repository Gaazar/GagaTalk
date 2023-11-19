#include "sqlite/sqlite3.h"
#include <stdio.h>
#include <assert.h>
#include <functional>
#include <map>
#include <string>
#include "client.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include "sql.h"

sqlite3* db = nullptr;
int db_version = -1;
char* err_msg = nullptr;
int upgrade()
{
	std::string sql;
	if (db_version == 1)
	{
		sql =
			"ALTER TABLE `config` rename to config_v1;\
			 CREATE TABLE `config` (\
			 name TEXT PRIMARY KEY NOT NULL,\
			 username TEXT DEFAULT 'Guest',\
			 filters TEXT DEFAULT '[{\"type\": \"RNNDenoise\",\"name\": \"Denoise\",\"enable\": true,\"args\": []}]',\
			 input TEXT DEFAULT NULL,\
			 output TEXT DEFAULT NULL,\
			 default_flags INT NOT NULL DEFAULT 3,\
			 mute INT NOT NULL DEFAULT 0,\
			 volume REAL NOT NULL DEFAULT 0.0,\
			 current INT NOT NULL DEFAULT 1);\
			 INSERT INTO `config` select name,username,filters,input,ouput,default_flags,mute,volume,current FROM config_v1;\
			 UPDATE meta set version = 2 where id = 0;";
		db_version = 2;
		int r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);
		assert(!r);
	}
	if (db_version == 2)
	{
		sql =
			"\
			ALTER TABLE config ADD COLUMN sapi TEXT DEFAULT NULL;\
			UPDATE meta set version = 3 where id = 0;\
			";
		db_version = 3;
		int r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);
		assert(!r);
	}
	if (db_version == 3)
	{
		sql =
			"\
			ALTER TABLE config ADD COLUMN silent INT DEFAULT 0;\
			UPDATE meta set version = 4 where id = 0;\
			";
		db_version = 4;
		int r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);
		assert(!r);
	}
	if (db_version == 4)
	{
		sql =
			"\
			ALTER TABLE config ADD COLUMN sapi TEXT DEFAULT NULL;\
			UPDATE meta set version = 5 where id = 0;\
			";
		db_version = 5;
		int r = sqlite3_exec(db, sql.c_str(), nullptr, 0, &err_msg);
		assert(!r);
	}
	return 0;
}
int create_structure()
{
	const char* sql;
	sql =
		"CREATE TABLE IF NOT EXISTS meta(\
		id INT PRIMARY KEY NOT NULL,\
		version INT NOT NULL,\
		app_ver INT NOT NULL\
		);";
	int r = sqlite3_exec(db, sql, nullptr, 0, &err_msg);
	assert(!r);
	_map kv;
	sql = "SELECT * FROM meta WHERE id = 0;";
	r = sqlite3_exec(db, sql, &kv, &err_msg);
	assert(!r);
	if (kv.size() == 0)
	{
		//sql = "UPDATE meta SET version = 1, app_ver = 1 where id = 0;";
		sql = "INSERT INTO meta (id,version,app_ver) VALUES (0,5,1);";
		r = sqlite3_exec(db, sql, nullptr, 0, &err_msg);
		assert(!r);
	}
	else
		db_version = atoi(kv["version"].c_str());
	kv.clear();

	sql =
		"CREATE TABLE IF NOT EXISTS servers(\
		suid INT NOT NULL,\
		name TEXT NOT NULL,\
		address TEXT PRIMARY KEY NOT NULL,\
		icon TEXT DEFAULT NULL,\
		token TEXT DEFAULT NULL\
		);";
	r = sqlite3_exec(db, sql, nullptr, 0, &err_msg);
	assert(!r);

	sql =
		"CREATE TABLE IF NOT EXISTS users_config(\
		suid INT NOT NULL,\
		server TEXT NOT NULL,\
		volume REAL NOT NULL DEFAULT 0.0,\
		mute INT NOT NULL DEFAULT 0\
		);";
	r = sqlite3_exec(db, sql, nullptr, 0, &err_msg);
	assert(!r);

	sql =
		"CREATE TABLE IF NOT EXISTS config(\
		name TEXT PRIMARY KEY NOT NULL,\
		username TEXT DEFAULT 'Guest',\
		filters TEXT DEFAULT '[{\"type\": \"RNNDenoise\",\"name\": \"Denoise\",\"enable\": true,\"args\": []}]',\
		input TEXT DEFAULT NULL,\
		output TEXT DEFAULT NULL,\
		default_flags INT NOT NULL DEFAULT 3,\
		mute INT NOT NULL DEFAULT 0,\
		silent INT NOT NULL DEFAULT 0,\
		volume REAL NOT NULL DEFAULT 0.0,\
		sapi TEXT DEFAULT NULL,\
		current INT NOT NULL DEFAULT 1\
		);";
	r = sqlite3_exec(db, sql, nullptr, 0, &err_msg);
	assert(!r);
	int c = 0;

	sql =
		"SELECT COUNT(*) from config;";
	r = sqlite3_exec(db, sql, [&](int cnt, char** v, char** k)->int
		{
			c = atoi(*v);
			return 0;
		}, &err_msg);

	if (c == 0)
	{
		sql =
			"INSERT INTO config (name) VALUES ('default');";
		r = sqlite3_exec(db, sql, nullptr, 0, &err_msg);
		assert(!r);

	}
	assert(!r);

	upgrade();
	return r;
}
int configs_init()
{
	if (db) return 1;
	int r = sqlite3_open("config.sq3", &db);
	if (r) return r;
	create_structure();
}
int configs_uninit()
{
	return 0;
}
int release_configurations()
{
	sqlite3_close(db);
	db = nullptr;
	return 0;
}
int conf_get_config(config* cfg)
{
	return 0;
}
int conf_set_config(config* cfg)
{
	return 0;
}
int conf_audio_set_volume(float vol)
{
	return 0;
}
int conf_audio_set_mute(bool m)
{
	return 0;
}
int conf_get_servers(std::vector<server_info>& s)
{
	s.clear();
	char* emsg = nullptr;
	char sql[] = "SELECT * FROM servers;";
	auto hr = sqlite3_exec(db, sql, [&](_cmap& kv)
		{
			server_info si;
			si.hostname = kv["address"];
			si.name = kv["name"];
			si.suid = stru64(kv["suid"]);
			auto x = kv["icon"];
			if (x) si.icon = x;
			x = kv["token"];
			if (x) si.token = x;
			s.push_back(si);
		}, &emsg);
	assert(!hr);
	return 0;
}

int conf_get_server(server_info* s) //fill hostname and others will be filled.
{
	if (!s) return -1;
	char* emsg = nullptr;
	auto sql = fmt::format("SELECT * FROM servers WHERE address = '{}';", s->hostname);
	_map kv;
	auto hr = sqlite3_exec(db, sql.c_str(), &kv, &emsg);
	assert(!hr);
	if (kv.size() == 0)
	{
		s->suid = std::to_string(randu32());
		s->name = s->hostname;
		sql = fmt::format("INSERT INTO servers (name,address,suid) VALUES ('{}','{}',{});", s->name, s->hostname, s->suid);
		hr = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &emsg);
		assert(!hr);
	}
	else
	{
#ifndef _DEBUG
		srand(time(nullptr));
		s->suid = std::to_string(randu32());// kv["suid"];
#else
		s->suid = kv["suid"];
#endif
		s->name = kv["name"];
		s->hostname = kv["address"];
		s->icon = kv["icon"];
		s->token = kv["token"];
	}
	return 0;
}
int conf_set_token(std::string host, uint64_t suid, std::string token)
{
	auto sql = fmt::format("UPDATE servers SET suid = {}, token = '{}' WHERE address = '{}';", suid, token, host);
	auto hr = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
	assert(!hr);
	return hr;
}
int conf_get_username(std::string& un)//return 1 if success
{
	char* emsg = nullptr;
	auto sql = fmt::format("SELECT username FROM config WHERE current = 1;");
	_map kv;
	auto hr = sqlite3_exec(db, sql.c_str(), &kv, &emsg);
	assert(!hr);
	if (kv.size())
	{
		un = kv["username"];
		return 1;
	}
	return 0;
}
int conf_set_username(std::string un)//return 1 if success
{
	char* emsg = nullptr;
	auto sql = fmt::format("UPDATE config SET username = '{}' WHERE current = 1;", un);
	auto hr = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &emsg);
	assert(!hr);
	return 0;
}

int conf_exec_sql(const char* sql, std::function<int(int, char**, char**)> func, char** errmsg)
{
	if (!db) return -1;
	return sqlite3_exec(db, sql, func, errmsg);
}
int conf_exec_sql(const char* sql, _cb_map func, char** errmsg)
{
	if (!db) return -1;
	return sqlite3_exec(db, sql, func, errmsg);
}

int conf_get_filter(std::string& filter)
{
	char* emsg = nullptr;
	auto sql = fmt::format("SELECT filters FROM config WHERE current = 1 limit 1;");
	_map kv;
	auto hr = sqlite3_exec(db, sql.c_str(), &kv, &emsg);
	assert(!hr);
	if (kv.size())
	{
		filter = kv["filters"];
		return 1;
	}
	return 0;

}
int conf_set_filter(std::string filter)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("UPDATE config SET filters = %Q WHERE current = 1;", filter.c_str());
	auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	return 0;
}

bool conf_set_input_device(std::string* devid)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("UPDATE config SET input = %Q WHERE current = 1;", devid ? devid->c_str() : nullptr);
	auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	return 0;
}
bool conf_set_output_device(std::string* devid)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("UPDATE config SET output = %Q WHERE current = 1;", devid ? devid->c_str() : nullptr);
	auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	return 0;
}
bool conf_get_input_device(std::string& devid)
{
	char* emsg = nullptr;
	auto sql = fmt::format("SELECT input FROM config WHERE current = 1 limit 1;");
	_cmap kv;
	int ret = 0;
	auto hr = sqlite3_exec(db, sql.c_str(), [&](_cmap& kv)
		{
			if (kv.size() && kv["input"])
			{
				devid = kv["input"];
				ret = 1;
			}
		}, &emsg);
	assert(!hr);
	return ret;

}
bool conf_get_output_device(std::string& devid)
{
	char* emsg = nullptr;
	auto sql = fmt::format("SELECT output FROM config WHERE current = 1 limit 1;");
	int ret = 0;
	_cmap kv;
	auto hr = sqlite3_exec(db, sql.c_str(), [&](_cmap& kv)
		{
			if (kv.size() && kv["output"])
			{
				devid = kv["output"];
				ret = 1;
			}
		}, &emsg);
	assert(!hr);

	return ret;

}
int conf_set_uc_volume(std::string host, uint32_t suid, float v)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT COUNT(*) FROM users_config WHERE suid = %u and server = %Q;", suid, host.c_str());
	_map kv;
	auto hr = sqlite3_exec(db, sql, &kv, &emsg);
	sqlite3_free(sql);
	if (kv["COUNT(*)"] == "0")
	{
		sql = sqlite3_mprintf("INSERT INTO users_config (suid,server,volume) VALUES (%u,%Q,%Q);", suid, host.c_str(), std::to_string(v).c_str());
		auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
		sqlite3_free(sql);
		assert(!hr);
	}
	else
	{
		sql = sqlite3_mprintf("UPDATE users_config SET volume = %Q WHERE suid = %u and server = %Q;", std::to_string(v).c_str(), suid, host.c_str());
		auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
		sqlite3_free(sql);
		assert(!hr);
	}
	return 1;
}
int conf_get_uc_volume(std::string host, uint32_t suid, float& v)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT volume FROM users_config WHERE suid = %u and server = %Q limit 1;", suid, host.c_str());
	_map kv;
	auto hr = sqlite3_exec(db, sql, &kv, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	v = 0.0f;
	if (kv.size())
	{
		v = std::strtof(kv["volume"].c_str(), nullptr);
		return 1;
	}
	return 0;
}
int conf_set_uc_mute(std::string host, uint32_t suid, bool m)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT COUNT(*) FROM users_config WHERE suid = %u and server = %Q;", suid, host.c_str());
	_map kv;
	auto hr = sqlite3_exec(db, sql, &kv, &emsg);
	sqlite3_free(sql);
	if (kv["COUNT(*)"] == "0")
	{
		sql = sqlite3_mprintf("INSERT INTO users_config (suid,server,mute) VALUES (%u,%Q,%d);", suid, host.c_str(), m);
		auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
		sqlite3_free(sql);
		assert(!hr);
	}
	else
	{
		sql = sqlite3_mprintf("UPDATE users_config SET mute = %d WHERE suid = %u and server = %Q;", m, suid, host.c_str());
		auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
		sqlite3_free(sql);
		assert(!hr);
	}
	return 1;
}
int conf_get_uc_mute(std::string host, uint32_t suid, bool& m)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT mute FROM users_config WHERE suid = %u and server = %Q limit 1;", suid, host.c_str());
	_map kv;
	auto hr = sqlite3_exec(db, sql, &kv, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	m = false;
	if (kv.size())
	{
		m = stru64(kv["mute"]);
		return 1;
	}
	return 0;
}
int conf_set_global_volume(float vol)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("UPDATE config SET volume = %Q WHERE current = 1;", std::to_string(vol).c_str());
	auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	return 1;
}
int conf_set_global_mute(bool m)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("UPDATE config SET mute = %d WHERE current = 1;", m);
	auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	return 1;
}
int conf_set_global_silent(bool m)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("UPDATE config SET silent = %d WHERE current = 1;", m);
	auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	return 1;
}
int conf_get_global_volume(float& vol)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT volume FROM config WHERE current = 1 limit 1;");
	_map kv;
	auto hr = sqlite3_exec(db, sql, &kv, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	vol = 0.0f;
	if (kv.size())
	{
		vol = std::strtof(kv["volume"].c_str(), nullptr);
		return 1;
	}
	return 0;
}
int conf_get_global_mute(bool& m)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT mute FROM config WHERE current = 1 limit 1;");
	_map kv;
	auto hr = sqlite3_exec(db, sql, &kv, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	m = false;
	if (kv.size())
	{
		m = stru64(kv["mute"]);
		return 1;
	}
	return 0;
}
int conf_get_global_silent(bool& m)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT silent FROM config WHERE current = 1 limit 1;");
	_map kv;
	auto hr = sqlite3_exec(db, sql, &kv, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	m = false;
	if (kv.size())
	{
		m = stru64(kv["silent"]);
		return 1;
	}
	return 0;
}
int conf_set_exit_check(bool dc)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT default_flags FROM config WHERE current = 1 limit 1;");
	_map kv;
	auto hr = sqlite3_exec(db, sql, &kv, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	uint64_t f = 0b011;
	if (kv.size())
	{
		f = stru64(kv["default_flags"]);
	}
	f = (f & (~0b100)) | (dc ? 0b100 : 0b000);
	sql = sqlite3_mprintf("UPDATE config SET default_flags = %d WHERE current = 1;", f);
	hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	return 1;
}
bool conf_get_exit_check()
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT default_flags FROM config WHERE current = 1 limit 1;");
	_map kv;
	auto hr = sqlite3_exec(db, sql, &kv, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	bool m = true;
	if (kv.size())
	{
		m = (stru64(kv["default_flags"]) & 0b100);
		return m;
	}
	return true;
}

int conf_get_sapi(std::string& s)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("SELECT sapi FROM config WHERE current = 1 limit 1;");
	_cmap kv;
	int ret = 0;
	s = "{}";
	auto hr = sqlite3_exec(db, sql, [&](_cmap& kv)
		{
			if (kv.size() && kv["sapi"])
			{
				s = kv["sapi"];
				ret = 1;
			}
		}, &emsg);
	assert(!hr);

	return ret;

}
int conf_set_sapi(std::string s)
{
	char* emsg = nullptr;
	auto sql = sqlite3_mprintf("UPDATE config SET sapi = %Q WHERE current = 1;", s.c_str());
	auto hr = sqlite3_exec(db, sql, nullptr, nullptr, &emsg);
	sqlite3_free(sql);
	assert(!hr);
	return 1;
}