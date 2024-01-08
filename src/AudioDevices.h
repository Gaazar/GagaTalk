#pragma once

#include <MMDeviceAPI.h>
#include <AudioClient.h>

#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <initguid.h>
#include <sapi.h>
#pragma comment(lib,"avrt.lib")

#include <configor/json.hpp>

enum class DeviceType
{
	Input,
	Output,
};

/*
return
[
	{
		"name":string, device friendly name
		"id":string, device id
		"type":string, input or output
		"default":string, empty string if it is not a default device, or communication media
	}
]
*/
configor::json EnumerateDevices(DeviceType type);