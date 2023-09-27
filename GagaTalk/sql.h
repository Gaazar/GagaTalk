#pragma once
#include "sqlite/sqlite3.h"
#include <stdio.h>
#include <assert.h>
#include <functional>
#include <map>
#include <string>

using _cb = std::function<int(int, char**, char**)>;
using _map = std::map<std::string, std::string>;
using _cmap = std::map<std::string, const char*>;
using _cb_map = std::function<void(_map&) >;
using _cb_cmap = std::function<void(_cmap&) >;
static int callback(void* pFunc, int argc, char** argv, char** azColName)
{
	_cb* func = (_cb*)pFunc;
	return func->operator()(argc, argv, azColName);
}
static int callback_map(void* pMap, int argc, char** argv, char** azColName)
{
	_map& kv = *(_map*)pMap;
	for (int i = 0; i < argc; i++)
	{
		if (argv[i])
			kv[azColName[i]] = argv[i];
		else
			kv[azColName[i]] = "";
	}
	return 0;
}
static int callback_map_func(void* pFunc, int argc, char** argv, char** azColName)
{
	_map kv;
	auto func = (_cb_map*)pFunc;
	for (int i = 0; i < argc; i++)
	{
		if (argv[i])
			kv[azColName[i]] = argv[i];
		else
			kv[azColName[i]] = "";
	}
	func->operator()(kv);
	return 0;
}
static int callback_cmap(void* pMap, int argc, char** argv, char** azColName)
{
	_cmap& kv = *(_cmap*)pMap;
	for (int i = 0; i < argc; i++)
	{
		kv[azColName[i]] = argv[i];
	}
	return 0;
}
static int callback_cmap_func(void* pFunc, int argc, char** argv, char** azColName)
{
	_cmap kv;
	auto func = (_cb_cmap*)pFunc;
	for (int i = 0; i < argc; i++)
	{
		kv[azColName[i]] = argv[i];
	}
	func->operator()(kv);
	return 0;
}

static int sqlite3_exec(sqlite3* db, const char* sql, std::function<int(int, char**, char**)> func, char** errmsg)
{

	return sqlite3_exec(db, sql, callback, &func, errmsg);
}
static int sqlite3_exec(sqlite3* db, const char* sql, _map* map, char** errmsg)
{
	return sqlite3_exec(db, sql, callback_map, map, errmsg);
}
static int sqlite3_exec(sqlite3* db, const char* sql, _cb_map map, char** errmsg)
{
	return sqlite3_exec(db, sql, callback_map_func, &map, errmsg);
}
//static int sqlite3_exec(sqlite3* db, const char* sql, _cmap* map, char** errmsg)
//{
//	return sqlite3_exec(db, sql, callback_cmap, map, errmsg);
//}
static int sqlite3_exec(sqlite3* db, const char* sql, _cb_cmap map, char** errmsg)
{
	return sqlite3_exec(db, sql, callback_cmap_func, &map, errmsg);
}