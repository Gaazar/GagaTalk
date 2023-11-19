#include<winsock2.h>
#include "gt_defs.h"
#include "client.h"
#pragma comment(lib, "ws2_32.lib")

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma warning(disable:4996)
#define FMT_HEADER_ONLY
#include <fmt/core.h>

struct plat_conn
{
	SOCKET sk_cmd;
	SOCKET sk_voip;
	std::thread th_cmd;
	std::thread th_voip;
	std::thread th_heartbeat;
	sockaddr_in addr_server;
	bool discard = false;

	~plat_conn()
	{
		th_heartbeat.detach();
	}
};

connection::connection()
{
	plat = new plat_conn;
	int e = 0;
	aud_enc = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &e);
}
int connection::connect(const char* host, uint16_t port)//sync
{
	this->host = host;
	name = host;
	plat->sk_cmd = socket(AF_INET, SOCK_STREAM, 0);
	if (!plat->sk_cmd) return -1;
	plat->sk_voip = socket(AF_INET, SOCK_DGRAM, 0);
	if (!plat->sk_voip) return -1;
	hostent* pHost = gethostbyname(host);
	if (!pHost)
	{
		//printf("Unable to resolve hostname:%s\n", host);
		closesocket(plat->sk_cmd);
		plat->sk_cmd = 0;
		closesocket(plat->sk_voip);
		plat->sk_voip = 0;
		return -2;
	}
	plat->addr_server.sin_family = AF_INET;
	plat->addr_server.sin_port = htons(port);
	CopyMemory(&plat->addr_server.sin_addr.S_un.S_addr, pHost->h_addr_list[0], pHost->h_length);

	SOCKADDR_IN saddr;
	saddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(0);
	if (bind(plat->sk_cmd, (SOCKADDR*)&saddr, sizeof(SOCKADDR)) != 0)
	{
		closesocket(plat->sk_cmd);
		plat->sk_cmd = 0;
		closesocket(plat->sk_voip);
		plat->sk_voip = 0;
		return -3;
	}
	saddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(0);
	if (bind(plat->sk_voip, (SOCKADDR*)&saddr, sizeof(SOCKADDR)) != 0)
	{
		closesocket(plat->sk_cmd);
		plat->sk_cmd = 0;
		closesocket(plat->sk_voip);
		plat->sk_voip = 0;
		return -3;
	}
	status = state::connecting;
	auto se = ::connect(plat->sk_cmd, (sockaddr*)&plat->addr_server, sizeof(plat->addr_server));
	if (se == SOCKET_ERROR) {
		//printf("Connect to server failed.\n");
		closesocket(plat->sk_cmd);
		plat->sk_cmd = 0;
		closesocket(plat->sk_voip);
		plat->sk_voip = 0;
		return -3;
	}
	se = handshake();
	status = state::verifing;
	plat->addr_server.sin_port = htons(17970);
	plat->th_cmd = std::thread([this]()
		{
			command_buffer cb;
			char buffer[1024];
			command cmd;
			CoInitialize(NULL);
			//int tv_out = 10000;
			//auto hr = setsockopt(plat->sk_cmd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv_out, sizeof(tv_out));
			while (!discard && !plat->discard)
			{
				int len = recv(plat->sk_cmd, buffer, sizeof buffer, 0);
				if (len > 0)
				{
					int n_cmd;
					if (n_cmd = cb.append(buffer, len))
					{
						for (int i = 0; i < n_cmd; i++)
						{
							int c = cb.parse(cmd);
							if (cmd.n_args())
							{
								if (cmd[0] == "pong") ping_pong--;
								on_recv_cmd(cmd);
							}
							cmd.clear();
						}
					}
				}
				//else if (len == EWOULDBLOCK || len == EAGAIN)
				//{
				//	printf("recv timeo, %s(%d)\n", strerror(errno), errno);
				//}
				else
				{
					printf("Disconnected: %s(%d)\n", strerror(errno), WSAGetLastError());
					disconnect();
					return -1;
				}
			}
			CoUninitialize();
			return 0;
		});
	plat->th_voip = std::thread([this]()
		{
			char buffer[1536];
			while (!discard && !plat->discard)
			{
				sockaddr_in from;
				memset(&from, 0, sizeof from);
				int salen = sizeof(struct sockaddr);
				int len = recvfrom(plat->sk_voip, buffer, 1536, 0, (sockaddr*)&from, &salen);
				if (from.sin_addr.S_un.S_addr == plat->addr_server.sin_addr.S_un.S_addr)
					on_recv_voip_pack(buffer, len);
			}
			return 0;

		});
	plat->th_heartbeat = std::thread([this]()
		{
			char cmd[] = "ping\n";
			while (!discard && !plat->discard)
			{
				std::this_thread::sleep_for(std::chrono::seconds(30));
				if (!discard && !plat->discard)
				{
					send(plat->sk_cmd, cmd, sizeof(cmd) - 1, 0);
					ping_pong++;
					if (ping_pong > 5)
					{
						printf("好像断连接了\n");
					}
				}
			}
			return 0;

		});

	return se;
}
int connection::handshake()
{
	server_info si;
	std::string uname;
	si.hostname = host;
	conf_get_server(&si);
	conf_get_username(uname);
	suid = std::strtoull(si.suid.c_str(), nullptr, 10);
	auto cmd = fmt::format("hs {} {} {}\n", suid, uname, si.token);
	send_command(cmd.c_str(), cmd.length());
	return 0;
}
int connection::send_command(const char* buf, int sz)
{
	if (!plat->sk_cmd) return false;
	return send(plat->sk_cmd, buf, sz, 0);

}
int connection::send_command(std::string c)
{
	if (!plat->sk_cmd) return false;
	return send(plat->sk_cmd, c.c_str(), c.length(), 0);

}int connection::send_voip_pack(const char* buf, int sz)
{
	if (!plat->sk_voip) return false;
	return sendto(plat->sk_voip, buf, sz, 0, (sockaddr*)&plat->addr_server, sizeof plat->addr_server);

}
int connection::disconnect()
{
	if (!plat) return -1;
	plat->discard = true;
	status = state::disconnect;
	if (plat->sk_cmd)
	{
		closesocket(plat->sk_cmd);
		plat->sk_cmd = 0;
	}
	if (plat->sk_voip)
	{
		closesocket(plat->sk_voip);
		plat->sk_voip = 0;
	}
	if (plat->th_voip.get_id() != std::thread::id())
		plat->th_voip.detach();
	if (plat->th_cmd.get_id() != std::thread::id())
		plat->th_cmd.detach();
	return 0;
}
connection::~connection()
{
	if (mic)
	{
		plat_delete_vr(mic);
		mic = nullptr;
	}
	opus_encoder_destroy(aud_enc);
	disconnect();
	if (plat)
		delete plat;
}

bool wsa = false;
int socket_init()
{
	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA data;
	auto hr = WSAStartup(sockVersion, &data);

	return hr;
}
int socket_uninit()
{

	WSACleanup();
	return 0;
}