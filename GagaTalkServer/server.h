#pragma once
#include <mutex>
#include <vector>
#include "gt_defs.h"
#include <thread>
#include "sql.h"
#include <map>
#include <unordered_set>

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
typedef sockaddr_in SOCKADDR_IN;
#endif
#define VERSION_SEQ 4
struct instance;
extern bool terminated;
//typedef std::unordered_set<std::string> pm_set;
struct pm_set
{
	std::unordered_set<std::string> permissions;
	int level = 0;
};
enum class r
{
	ok = 0,
	new_user,
	e_auth,
	e_state,
};
struct statistics
{
	uint64_t time_enter = 0;
	uint64_t vo_tx = 0, vo_rx = 0; //tx = client's sent receive by server; rx = server sent to client
	uint64_t cm_tx = 0, cm_rx = 0;
	uint32_t last_ssid = 0;
	uint16_t cmd_port = 0;
};
struct connection : RemoteClientDesc
{
	SOCKET sk_cmd;
	SOCKADDR_IN addr;
	uint32_t cert_code = 0;
	std::thread  th_cmd;
	instance* server;
	std::string role_server;
	std::string role_channel;
	int activated = 0;
	bool discard = false;
	client_state state;
	statistics stts;
	connection();
	void on_recv_cmd(command& cmd);
	int send_cmd(std::string s);
	int send_buffer(const char* buf, int sz);
	int recv_cmd_thread();
	void release();
	void send_channel_list();
	void send_clients_list();
	//void send_channel_info();
	void send_clients_info();
	void join_channel(uint32_t chid);
	std::stringstream& cg_state(std::stringstream& ss);//cg = command generation, arguments
	bool permission(std::string name);
	bool permission(std::string name, chid_t chid);
	bool permission(std::string name, chid_t chid, chid_t chid2);//true when both of chid permite

	std::stringstream& debug_output(std::stringstream& ss);
	char* debug_address(char* buf, int len);
};
struct channel : ChannelDesc
{

	uint32_t session_id;
	std::vector<connection*> clients;
	instance* inst = nullptr;
	bool blocked = false;
	void broadcast_voip_pak(const char* buf, int sz, suid_t sender);
	void broadcast_cmd(std::string cmd, connection* ignore = nullptr);

	std::stringstream& cgl_listinfo(std::stringstream& ss);//command line generation, generates a full command, not a part;

};
struct instance
{
	std::mutex m_conn;
	std::mutex m_man;
	std::mutex m_channel;
	std::map<chid_t, channel*> channels;
	std::map<suid_t, connection*> connections;
	std::vector<connection*> conn_verifing;
	std::map<std::string, pm_set> server_roles;
	std::map<std::string, pm_set> channel_roles;
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
	void broadcast(std::string cmd, connection* ignore = nullptr);
	void send_voip(sockaddr_in* sa, const char* buf, int sz);
	void verified_connection(connection* c);
	void on_man_cmd(command& cmd, connection* conn);

	void cl_move(suid_t suid, chid_t chid); //cl = client
	void ch_delete(chid_t chid, chid_t cl_move_to = 1); //ch = channel

	r db_check_user(uint32_t& suid, std::string& token, std::string& msg);
	int db_get_channel(channel* c);
	int db_get_roles();
	int db_get_role_server(connection* c);
	int db_get_role_channel(connection* c);
	int db_get_role_channel(suid_t suid, chid_t chid, std::string& role);
	int db_gen_role_key(suid_t suid, std::string s_role, std::string c_role, chid_t chid, std::string& code);
	int db_gen_role_key_tag(suid_t suid, std::string s_tag, std::string c_tag, chid_t chid, std::string& code);
	int db_grant(suid_t op, suid_t u, chid_t* cid, char* to_s, char* to_c);
	bool db_user_exist(suid_t suid);
	int db_delete_channel(chid_t chid);
	int db_get_channels(chid_t parent, std::vector<chid_t>& members);
	int db_allocate_chid();
	int db_create_channel(chid_t c, chid_t p, std::string name, std::string desc, chid_t owner);
	int db_update_channel(chid_t c, const char* name, const char* desc);
	void mp_grk(command& cmd, connection* conn); //mp = manage process
	void mp_grant(command& cmd, connection* conn);
	void mp_new_channel(command& cmd, connection* conn);
	void mp_mod_channel(command& cmd, connection* conn);
	void mp_del_channel(command& cmd, connection* conn);
	void mp_sql(command& cmd, connection* conn);
	void mp_mute(command& cmd, connection* conn);
	void mp_silent(command& cmd, connection* conn);
	void mp_move(command& cmd, connection* conn);
	void mp_debug(command& cmd, connection* conn);

	static void man_help(connection* conn);
	static void man_display(std::string s, connection* conn);
};
struct role_permission
{
	std::unordered_set<std::string> permissions;

};
struct voice_session
{
	std::map<chid_t, channel*> channels;
	unsigned short port;
};
int socket_init();
uint64_t randu64();
uint32_t randu32();
std::string token_gen();