#pragma once
#include "../delegate.h"
#include <string>
#include "client.h"

enum class event
{
	join, left, mute, silent, auth, fail,
	input_change,	//data = char* device_id
	output_change,	//data = char* device_id
	volume_change,	//data = float* db
	initiate,		//data = nullptr

};

extern delegate<void(event type, connection* conn)> e_server;
extern delegate<void(event type, channel* channel)> e_channel;
extern delegate<void(event type, entity* entity)> e_entity;
extern delegate<void(event type, void* data)> e_client;


