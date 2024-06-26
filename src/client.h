#pragma once
#include <string>
#include <vector>
#include "IAudioFrameProcessor.h"
#include "gt_defs.h"
#include "RingQueue.h"
#include <opus/opus.h>
#include <functional>
#include "cli.h"
#define BUILD_SEQ 28
struct debug_info
{
	uint32_t n_pak_recv = 0;
	uint32_t nb_pak_recv = 0;
	uint32_t n_pak_sent = 0;
	uint32_t nb_pak_sent = 0;
	uint32_t n_null_pb = 0;
	uint32_t n_pak_err_enc = 0;
	remote_shell* shell = nullptr;
};
struct connection;
struct audio_device
{
	std::string name;
	std::string id;
};
struct config
{
	std::string name;
	std::string username;
	std::string filters;
	std::string input_device;
	std::string output_device;
	int mute;
	float volume_db;
};
struct server_info : ServerDesc
{
	std::string suid;
	std::string token;
};
struct plat_conn;
struct voice_playback;
struct entity : RemoteClientDesc
{
	voice_playback* playback = nullptr;
	connection* conn = nullptr;
	client_state entity_state;
	debug_info debug_state;
	std::string server_role;
	std::string channel_role;
	pm_set permissions;
	float get_volume();
	float set_volume(float);
	bool get_mute();
	bool set_mute(bool);
	void remove_playback();
	void create_playback();
	voice_playback* move_playback();
	void load_profile();
	~entity();
};
struct channel : ChannelDesc
{
	connection* conn;
	std::vector<entity*> entities;
	void erase(entity* e);
	void erase(uint32_t suid);
	void join(entity* e, bool is_new = false);
	~channel();
};
struct voice_recorder;
struct recorder_ref
{
	std::function<void(AudioFrame*)> callback;
};

struct connection :ServerDesc
{
	plat_conn* plat = nullptr;
	uint64_t suid;
	std::map<uint32_t, channel*> channels;
	std::map<uint64_t, entity*> entities;
	uint32_t chid = 0;
	uint32_t ssid = 0;
	channel* current = nullptr;
	uint32_t cert = 0;
	recorder_ref* mic = nullptr;
	OpusEncoder* aud_enc;
	uint32_t ping_pong = 0;
	client_state entity_state;
	debug_info debug_state;
	FrameAligner fa_netopt;
	entity* local = nullptr;
	enum class state
	{
		disconnect = 0,
		connecting,
		verifing,
		established
	};
	state status = state::disconnect;
	int handshake();
	void channel_switch(uint32_t from, uint32_t to);//self channel switch, don't call it when remote clients switching

	connection();
	int connect(const char* host, uint16_t port);//sync
	int send_command(const char* buf, int sz);
	int send_command(std::string s);
	int send_voip_pack(const char* buf, int sz);
	int disconnect();
	int clean_up();
	~connection();

	void on_recv_cmd(command& cmd);
	void on_recv_voip_pack(const char* buffer, int len);
	void on_mic_pack(AudioFrame* f);

	void join_channel(uint32_t chid);
	void quit_channel();
	void switch_channel();
	void send_message();
	void send_media();
	void change_state();//afk, active, fuck me, busy
	void set_client_volume(uint32_t suid, float vol, bool save = true);
	float get_client_volume(uint32_t suid);
	void set_client_mute(uint32_t suid, bool mute, bool save = true);
	bool get_client_mute(uint32_t suid);
	void tick();
	void set_netopt_paksz(uint32_t paksz);

};
int client_init();
int configs_init();
int platform_init();
int socket_init();
int client_uninit();
int configs_uninit();
int platform_uninit();
int socket_uninit();

voice_playback* plat_create_vpb();
void plat_delete_vpb(voice_playback*);
recorder_ref* plat_create_vr();
void plat_delete_vr(recorder_ref*);
bool plat_set_input_device(std::string devid);
std::string plat_get_input_device();
bool plat_set_output_device(std::string devid);
std::string plat_get_output_device();
bool plat_set_filter(std::string filter);
float plat_set_global_volume(float db);
bool plat_set_global_mute(bool m);
bool plat_set_global_silent(bool m);
float plat_get_global_volume();
bool plat_get_global_mute();
bool plat_get_global_silent();
bool plat_enum_input_device(std::vector<audio_device>& ls);
bool plat_enum_output_device(std::vector<audio_device>& ls);

void client_set_global_mute(bool m, bool broadcast = true);
void client_set_global_silent(bool m, bool broadcast = true);
void client_set_global_volume(float db);

void sapi_set_volume(int v);
void sapi_set_rate(int r);
int sapi_init();
int sapi_uninit();
void sapi_set_profile(std::string s);
std::string sapi_get_profile();


void client_check_update();

int w2a(const wchar_t* in, int len, char** out);
int a2w(const char* in, int len, wchar_t** out);
int conf_get_server(server_info* s); //fill hostname and others will be filled.
int conf_get_username(std::string& un); //return 1 if success
int conf_set_token(std::string host, uint64_t suid, std::string token);
int conf_set_server(server_info& si);//set name icon desc
int conf_set_server(ServerDesc& si);//set name icon desc
int conf_set_username(std::string un); //return 1 if success
int conf_get_filter(std::string& filter);
int conf_set_filter(std::string filter);
bool conf_set_input_device(std::string* devid);
bool conf_set_output_device(std::string* devid);
bool conf_get_input_device(std::string& devid);
bool conf_get_output_device(std::string& devid);
int conf_set_uc_volume(std::string host, uint32_t suid, float v);
int conf_get_uc_volume(std::string host, uint32_t suid, float& v);
int conf_set_uc_mute(std::string host, uint32_t suid, bool m);
int conf_get_uc_mute(std::string host, uint32_t suid, bool& m);
int conf_set_global_volume(float db);
int conf_set_global_mute(bool m);
int conf_set_global_silent(bool m);
int conf_get_global_volume(float& db);
int conf_get_global_mute(bool& m);
int conf_get_global_silent(bool& m);
int conf_get_sapi(std::string& s);
int conf_set_sapi(std::string s);
int conf_get_servers(std::vector<server_info>& s);
int conf_set_exit_check(bool dc);
bool conf_get_exit_check();

int sapi_say(std::string s);
int sapi_cancel();
int sapi_disable();
int sapi_enable();
std::string debug_devinfo();

int tray_init();
int tray_uninit();

int client_connect(std::string host, uint16_t port, connection** conn);
int client_disconnect(connection* conn);
connection* client_get_current_server(); //return hostname:port

void enable_voice_loopback();
void disable_voice_loopback();
bool toggle_voice_loopback();

uint64_t randu64();
uint32_t randu32();
