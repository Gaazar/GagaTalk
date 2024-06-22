#pragma once
#include "../sql.h"
int db_create_structure(sqlite3* db, char** msg);
int db_upgrade(sqlite3* db);