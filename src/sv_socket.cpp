#ifdef _MSC_VER
#include<winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable:4996)
#endif
#ifdef __GNUC__
typedef int SOCKET;
#endif
#include "gt_defs.h"


SOCKET sk_lsn = 0;
SOCKET sk_voip = 0;
struct plat_conn
{
	SOCKET sk_cmd;


};

int socket_init()
{
#ifdef _MSC_VER
	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA data;
	auto hr = WSAStartup(sockVersion, &data);
	return hr;
#endif
	return 0;
}

