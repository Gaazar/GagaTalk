#pragma once
#include <string>

int sapi_join_channel(std::string name);
int sapi_left_channel(std::string name);
int sapi_join_server(std::string name);
int sapi_left_server(std::string name);
