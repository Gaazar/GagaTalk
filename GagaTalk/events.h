#pragma once
#include "delegate.h"
#include <string>
#include "client.h"


extern delegate<void(std::string type, connection* conn)> e_server;
extern delegate<void(std::string type, channel* channel)> e_channel;
extern delegate<void(std::string type, entity* entity)> e_entity;


