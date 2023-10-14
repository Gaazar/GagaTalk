#include "db.h"
#define FMT_HEADER_ONLY
#include <fmt/core.h>
#include <functional>
#include <map>
#include <assert.h>
#include "sql.h"

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
int db_get_privilege(uint32_t suid)
{
	return 0;
}
int db_new_channel()
{
	return 0;
}
int db_delete_channel()
{
	return 0;
}
int db_get_channel()
{
	return 0;
}
int db_set_channel()
{
	return 0;
}
int db_set_channel_name()
{
	return 0;
}
int db_set_channel_desc()
{
	return 0;
}
int db_set_channel_opus()
{
	return 0;
}
