#pragma once
#include <string>
#include <vector>
#include "IAudioFrameProcessor.h"
#include "gt_defs.h"
#include "RingQueue.h"
#include <opus/opus.h>
#include <functional>

#define BUILD_SEQ 11 
struct connection;
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
struct server_info
{
	std::string suid;
	std::string name;
	std::string hostname;
	std::string icon;
	std::string token;
};
struct plat_conn;
struct voice_playback;
struct entity : RemoteClientDesc
{
	voice_playback* playback = nullptr;
	connection* conn = nullptr;
	uint32_t n_pak = 0;
	float get_volume();
	float set_volume(float);
	bool get_mute();
	bool set_mute(bool);
	void remove_playback();
	void create_playback();
	voice_playback* move_playback();
	void load_profile();
};
struct channel : ChannelDesc
{
	connection* conn;
	std::vector<entity*> entities;
	void erase(entity* e);
	void erase(uint32_t suid);
	void join(entity* e, bool is_new = false);
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
	int send_voip_pack(const char* buf, int sz);
	int disconnect();
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
bool plat_set_output_device(std::string devid);
bool plat_set_filter(std::string filter);
float plat_set_global_volume(float db);
bool plat_set_global_mute(bool m);
bool plat_set_global_silent(bool m);
float plat_get_global_volume();
bool plat_get_global_mute();
bool plat_get_global_silent();
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

int sapi_say(std::string s);
int sapi_cancel();
int sapi_disable();
int sapi_enable();


void enable_voice_loopback();
void disable_voice_loopback();

uint64_t randu64();
uint32_t randu32();
