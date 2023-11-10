#pragma once
#include <mutex>
#include <vector>
#include "gt_defs.h"
#include <thread>
#include "sql.h"
#include <map>

#ifdef _MSC_VER
#include<winsock2.h>
#endif
#ifdef __GNUC__
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET;
typedef SOCKADDR_IN sockaddr_in;
#endif
struct instance;
extern bool terminated;
enum class r
{
	ok = 0,
	new_user,
	e_auth,
	e_state,
};
struct connection : RemoteClientDesc
{
	SOCKET sk_cmd;
	SOCKADDR_IN addr;
	uint32_t cert_code;
	std::thread  th_cmd;
	instance* server;
	int activated = 0;
	bool discard = false;

	connection();
	void on_recv_cmd(command& cmd);
	int send_cmd(std::string s);
	int send_buffer(const char* buf, int sz);
	int recv_cmd_thread();
	void release();
	void send_channel_info();
	void send_clients_info();
	void join_channel(uint32_t chid);
};
struct channel : ChannelDesc
{

	uint32_t session_id;
	std::vector<connection*> clients;
	instance* inst = nullptr;
	void broadcast_voip_pak(const char* buf, int sz, uint32_t ignore_suid);

};
struct instance
{
	std::mutex m_conn;
	std::map<uint32_t, channel*> channels;
	std::map<uint32_t, connection*> connections;
	std::vector<connection*> conn_verifing;
	SOCKET sk_lsn;
	SOCKET sk_voip;
	std::thread  th_listen;
	std::thread  th_voip;
	sqlite3* db;
	char** db_msg = nullptr;
	bool discard = false;
	uint64_t last_clean = 0;

	int start(const char* db = "database.db3");
	int listen_thread();
	int voip_recv_thread();
	void broadcast(const char* buf, int sz, connection* ignore = nullptr);
	void send_voip(sockaddr_in* sa, const char* buf, int sz);
	void verified_connection(connection* c);
	void on_man_cmd(command& cmd);

	r db_check_user(uint32_t& suid, std::string& token, std::string& msg);
	int db_get_privilege(suid_t suid);
	int db_create_channel(ChannelDesc& cd);
	int db_delete_channel(chid_t id);

};

int socket_init();
uint64_t randu64();
uint32_t randu32();
std::string token_gen();