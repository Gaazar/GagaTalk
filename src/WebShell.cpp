#define CROW_STATIC_DIRECTORY "dist/"
#include <configor/json.hpp>
#include <crow.h>
#include <crow/middlewares/cors.h>
#include "web.h"
#include <filesystem>
#include "client.h"
#include "events.h"
#include "native_util.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include <windows.h>
#include <tchar.h>
#include <assert.h>
#include <shellapi.h>
void webapp_thread(int port);
void web_wsevent_send(std::string event);
void web_wsevent_send(configor::json event);

typedef crow::App<crow::CORSHandler> WebUI;
WebUI* web_app = nullptr;
std::thread web_th;
crow::websocket::connection* web_wsc = nullptr;

std::string to_string(configor::json j)
{
	return sutil::w2s(sutil::s2w(j.dump<configor::encoding::ignore>(), CP_ACP), CP_UTF8);
}
configor::json make_initiate(connection* conn);
configor::json make_server_info(connection* conn);
configor::json make_entitys_info(connection* conn);
configor::json make_client();
configor::json make_entity(entity* e);

void web_event_client(event e, void* conn);
void web_event_server(event e, connection* conn);
void web_event_channel(event e, channel* conn);
void web_event_entity(event e, entity* conn);
int web_init(int port)
{
	web_app = new WebUI;
	auto& app = *web_app;
	auto& cors = app.get_middleware<crow::CORSHandler>();
	cors
		.global()
		//.origin("https://gagarun.chat,http://127.0.0.1:10709,http://localhost:8080,http://127.0.0.1:8080")
		.origin("*")
		;

	CROW_ROUTE(app, "/")([]() {
		crow::response response;
		response.set_static_file_info("./dist/index.html");
		return response;
		});

	CROW_ROUTE(app, "/api/servers")([]() {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		response.code = 200;

		std::vector<server_info> tr_svrs;
		conf_get_servers(tr_svrs);
		configor::json res = configor::json::array({});
		for (auto& i : tr_svrs)
		{
			configor::json item;
			item["name"] = i.name;
			item["host"] = i.host + ":7970";
			item["icon"] = i.icon;
			res.push_back(std::move(item));
		}
		response.write(to_string(res));
		return response;
		});

	CROW_ROUTE(app, "/api/server")([]() {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		if (!client_get_current_server())
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10002},{"msg","no active server."} }).dump());
			return response;
		}
		response.write(to_string(make_server_info(client_get_current_server())));
		return response;

		return response;
		});

	CROW_ROUTE(app, "/api/initiate").methods("GET"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		if (!client_get_current_server())
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10002},{"msg","no active server."} }).dump());
			return response;
		}
		response.write(to_string(make_initiate(client_get_current_server())));
		return response;
		});

	CROW_ROUTE(app, "/api/server/connect").methods("POST"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto& p = req.get_body_params();
		auto c = client_get_current_server();
		if (!p.get("host"))
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10001},{"msg","host is not provided."} }).dump());
			return crow::response();
		}
		if (c && c->status == connection::state::established)
		{
			client_disconnect(c);
		}
		std::string host = p.get("host");
		auto i = host.find(':');
		uint16_t port = 7970;
		std::string hostname = host.substr(0, i);
		if (i != std::string::npos)
		{
			port = stru64(host.substr(i + 1));
		}
		if (c && c->host == host)
		{
			response.write(crow::json::wvalue({ {"code",0},{"msg","already"} }).dump());
			return response;
		}
		client_connect(hostname, port, &c);
		response.write(crow::json::wvalue({ {"code",0},{"msg","ok"} }).dump());

		return response;
		});

	CROW_ROUTE(app, "/api/server/disconnect").methods("POST"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto c = client_get_current_server();
		if (c) c->disconnect();
		response.write(crow::json::wvalue({ {"code",0},{"msg","ok"} }).dump());
		return response;
		});

	CROW_ROUTE(app, "/api/channel/join").methods("POST"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto& p = req.get_body_params();
		auto c = client_get_current_server();
		if (!c)
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10002},{"msg","no active server."} }).dump());
			return response;
		}
		if (!p.get("chid"))
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10001},{"msg","chid is not provided."} }).dump());
			return response;
		}
		c->join_channel(stru64(p.get("chid")));
		response.write(crow::json::wvalue({ {"code",0},{"msg","ok"} }).dump());
		return response;
		});
	CROW_ROUTE(app, "/api/channel/left").methods("POST"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto c = client_get_current_server();
		if (!c)
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10002},{"msg","no active server."} }).dump());
			return response;

		}
		c->join_channel(0);
		response.write(crow::json::wvalue({ {"code",0},{"msg","ok"} }).dump());
		return response;
		});

	CROW_ROUTE(app, "/api/client/audio").methods("GET"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto c = client_get_current_server();
		if (!c)
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10002},{"msg","no active server."} }).dump());
			return response;
		}

		configor::json j;
		j["entities"] = make_entitys_info(c);
		j["client"] = make_client();

		response.write(to_string(j));
		return response;
		});
	CROW_ROUTE(app, "/api/client/audio").methods("POST"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto& p = req.get_body_params();
		char* t = p.get("target"), * v = p.get("volume"), * m = p.get("mute"), * s = p.get("silent");
		if (t == std::string("master"))
		{
			if (v) client_set_global_volume(percent_to_db(stru64(v) / 100.f));
			if (m) client_set_global_mute(m == std::string("true"));
			if (s) client_set_global_silent(s == std::string("true"));
		}
		else
		{
			suid_t suid = stru64(t);
			auto c = client_get_current_server();
			if (!c)
			{
				response.write(crow::json::wvalue({ {"code",10002},{"msg","no active server."} }).dump());
				return response;
			}
			if (c->local->suid == suid)
			{
				response.write(crow::json::wvalue({ {"code",11005},{"msg","local."} }).dump());
				return response;
			}
			if (v) c->set_client_volume(suid, percent_to_db(stru64(v) / 100.f));
			if (m) c->set_client_mute(suid, m == std::string("true"));
			if (s)
			{
				response.code = 400;
				response.write(crow::json::wvalue({ {"code",11000},{"msg","option not allowed."} }).dump());
				return response;
			}
		}

		//std::cout << fmt::format("SetAudioConfig:\n\ttarget:{}\n\tvolume:{}\n\tmute:{}\n\tsilent:{}\n",
		//	t, v, m, s);
		return response;
		});

	CROW_ROUTE(app, "/api/client/input").methods("POST"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto& p = req.get_body_params();
		char* id = p.get("id");
		std::string sid = id;
		if (!id)
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10001},{"msg","id is not provided."} }).dump());
			return response;
		}
		if (plat_set_input_device(id))
		{
			conf_set_input_device(&sid);
			return response;
		}
		//std::cout << fmt::format("SetAudioConfig:\n\ttarget:{}\n\tvolume:{}\n\tmute:{}\n\tsilent:{}\n",
		//	t, v, m, s);
		response.code = 400;
		response.write(crow::json::wvalue({ {"code",11002},{"msg","id not avaliable."} }).dump());
		});
	CROW_ROUTE(app, "/api/client/output").methods("POST"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto& p = req.get_body_params();
		char* id = p.get("id");
		std::string sid = id;
		if (!id)
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10001},{"msg","id is not provided."} }).dump());
			return response;
		}
		if (plat_set_output_device(id))
		{
			conf_set_output_device(&sid);
			return response;
		}
		//std::cout << fmt::format("SetAudioConfig:\n\ttarget:{}\n\tvolume:{}\n\tmute:{}\n\tsilent:{}\n",
		//	t, v, m, s);
		response.code = 400;
		response.write(crow::json::wvalue({ {"code",11002},{"msg","id not avaliable."} }).dump());
		});
	CROW_ROUTE(app, "/api/client/username").methods("POST"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto& p = req.get_body_params();
		char* id = p.get("name");
		if (!id)
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10001},{"msg","name is not provided."} }).dump());
			return response;
		}
		conf_set_username(id);
		return response;
		});

	CROW_ROUTE(app, "/api/client/user").methods("GET"_method)([](const crow::request& req) {
		crow::response response;
		response.set_header("Content-Type", "application/json");
		auto c = client_get_current_server();
		if (!c)
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10002},{"msg","no active server."} }).dump());
			return response;
		}
		char* suid = req.url_params.get("suid");
		if (!suid)
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",10001},{"msg","suid is not provided."} }).dump());
			return response;
		}
		if (!c->entities.count(stru64(suid)))
		{
			response.code = 400;
			response.write(crow::json::wvalue({ {"code",11004},{"msg","suid dose not exists."} }).dump());
			return response;
		}

		response.write(to_string(make_entity(c->entities[stru64(suid)])));
		return response;
		});

	/*
	CROW_ROUTE(app, "/api/")([]() {
		crow::response response;
	response.set_header("Content-Type", "application/json");

		return response;
		});
	*/

	auto& wsrc = CROW_WEBSOCKET_ROUTE(app, "/api/notify");

	wsrc.onopen([&](crow::websocket::connection& conn)
		{
			if (web_wsc)
			{
				auto wsc = web_wsc;
				web_wsc = nullptr;
				//wsc->close();
				printf("wsraw reconnect\n");
			}
			else
			{
				printf("wsraw connect\n");
			}
			web_wsc = &conn;

		});
	wsrc.onclose([&](crow::websocket::connection& conn, const std::string& reason)
		{
			if (web_wsc == &conn)
			{
				printf("wsraw disconnect\n");

			}
		});
	wsrc.onmessage([&](crow::websocket::connection& /*conn*/, const std::string& data, bool is_binary)
		{
			printf("wsraw message(%s):\n%s\n", is_binary ? "bin" : "str", data.c_str());
		});


	CROW_ROUTE(app, "/<path>")([&](const std::string& p) {
		crow::response response;

		std::string path{ "./dist/" + p };

		if (!std::filesystem::exists(path)) {
			response.set_header("Content-Type", "application/json");
			response.code = 404;
			response.write("Not Found");
			return response;
		}

		response.set_static_file_info(path);
		return response;
		});
	app.loglevel(crow::LogLevel::Error);
	std::cout << "WebUI is running on http://127.0.0.1:10709" << std::endl;
	web_th = std::thread([=]() {
		webapp_thread(port);
		});

	e_client += web_event_client;
	e_server += web_event_server;
	e_channel += web_event_channel;
	e_entity += web_event_entity;

	web_open_browser("http://127.0.0.1:10709");
	return 0;
}
configor::json make_voice_info(entity* e)
{
	float vol = .0f;
	bool lm = false;
	int mute = 0;
	int silent = 0;
	if (e->playback)
	{
		vol = e->get_volume();
		lm = e->get_mute();
	}
	else
	{
		conf_get_uc_volume(e->conn->host, e->suid, vol);
		conf_get_uc_mute(e->conn->host, e->suid, lm);
	}
	mute = e->entity_state.mute_value();
	silent = e->entity_state.silent_value();
	return configor::json::object(
		{
			{"volume", db_to_percent(vol)},
			{"mute", mute},
			{"silent", silent},
			{"mute_local", lm}
		});
}
configor::json make_channels_info(connection* conn)
{
	configor::json channels;
	for (auto& i : conn->channels)
	{
		configor::json t =
		{
			{"chid", i.first},
			{"name", i.second->name},
			{"description", i.second->description},
			{"parent", i.second->parent}
		};
		channels.push_back(std::move(t));
	}
	return channels;
}
configor::json make_entitys_info(connection* conn)
{
	configor::json entities = { };
	for (auto& i : conn->entities)
	{
		entities.push_back(std::move(make_entity(i.second)));
	}
	return entities;
}
configor::json make_server_info(connection* conn)
{
	return configor::json(
		{
			{"name", conn->name},
			{"host", conn->host + ":7970"},
			{"description", "HTML, not impl."}
		});
}
configor::json make_permission(entity* e)
{
	configor::json j;
	for (auto& i : e->permissions.permissions)
	{
		j.push_back(i);
	}
	return j;
}
configor::json make_entity(entity* e)
{
	return configor::json({
		{"server_role", e->server_role},
		{"channel_role",  e->channel_role },
		{"current_channel", e->current_chid},
		{"permissions", make_permission(e)},
		{"name",e->name},
		{"suid",e->suid},
		{"pronounce", e->name},
		{"voice", make_voice_info(e)},
		{"avatar",e->avatar}
		});
}
configor::json make_client()
{
	configor::json j;
	float vol_db = 0;
	bool mute;
	bool silent;
	std::string inp;
	std::string out;
	std::string uname;
	conf_get_username(uname);
	conf_get_global_volume(vol_db);
	conf_get_global_mute(mute);
	conf_get_global_silent(silent);
	conf_get_input_device(inp);
	conf_get_output_device(out);
	j["volume"] = db_to_percent(vol_db);
	j["mute"] = mute;
	j["silent"] = silent;
	j["input_id"] = inp;
	j["output_id"] = out;
	j["username"] = uname;

	std::vector<audio_device> ins;
	std::vector<audio_device> ous;
	configor::json inputs;
	configor::json outputs;
	plat_enum_input_device(ins);
	plat_enum_output_device(ous);
	for (auto& i : ins)
	{
		inputs.push_back({ {"id",i.id},{"name",i.name} });
	}
	for (auto& i : ous)
	{
		outputs.push_back({ {"id",i.id},{"name",i.name} });
	}
	j["inputs"] = inputs;
	j["outputs"] = outputs;
	return j;
}
configor::json make_initiate(connection* conn)
{
	configor::json j;
	j["type"] = "initiate";
	j["channels"] = make_channels_info(conn);
	j["entities"] = make_entitys_info(conn);
	j["server"] = make_server_info(conn);
	j["local"] = make_entity(conn->local);
	j["client"] = make_client();
	return j;
}


void web_event_server(event e, connection* conn)
{
	switch (e)
	{
	case event::join://yes
		web_wsevent_send(configor::json({ {"type","join_server"} }));
		break;
	case event::left://yes
		web_wsevent_send(configor::json({ {"type","left_server"} }));
		break;
	case event::mute:
		break;
	case event::silent:
		break;
	case event::auth://yes
		web_wsevent_send(configor::json({ {"type","auth"} }));
		break;
	case event::input_change:
		break;
	case event::output_change:
		break;
	case event::volume_change:
		break;
	case event::initiate://yes
	{
		web_wsevent_send(configor::json({ {"type","initiate"} }));
		break;
	}
	case event::fail:
		web_wsevent_send(configor::json({ {"type","fail_connect"} }));
		break;
		break;
	default:
		break;
	}
}
void web_event_client(event e, void* c)
{
	switch (e)
	{
	case event::join:
		break;
	case event::left:
		break;
	case event::mute://
		web_wsevent_send(configor::json({ {"type","mute_local"} }));
		break;
	case event::silent://
		web_wsevent_send(configor::json({ {"type","silent_local"} }));
		break;
	case event::auth:
		break;
	case event::input_change:
		web_wsevent_send(configor::json({ {"type","input"} }));
		break;
	case event::output_change:
		web_wsevent_send(configor::json({ {"type","output"} }));
		break;
	case event::volume_change:
		web_wsevent_send(configor::json({ {"type","volume_master"} }));
		break;
	case event::initiate:
		break;
	default:
		break;
	}
}
void web_event_channel(event e, channel* c)
{
	switch (e)
	{
	case event::join://
		web_wsevent_send(configor::json({ {"type","join_channel_local"} }));
		break;
	case event::left://
		web_wsevent_send(configor::json({ {"type","left_channel_local"} }));
		break;
	case event::mute:
		break;
	case event::silent:
		break;
	case event::auth:
		break;
	case event::input_change:
		break;
	case event::output_change:
		break;
	case event::volume_change:
		break;
	case event::initiate:
		break;
	default:
		break;
	}
}
void web_event_entity(event e, entity* conn)
{
	switch (e)
	{
	case event::join://
		web_wsevent_send(configor::json({ {"type","join_channel"} }));
		break;
	case event::left://
		web_wsevent_send(configor::json({ {"type","join_channel"} }));
		break;
	case event::mute://
		web_wsevent_send(configor::json({ {"type","mute"} }));
		break;
	case event::silent://
		web_wsevent_send(configor::json({ {"type","silent"} }));
		break;
	case event::auth:
		break;
	case event::input_change:
		break;
	case event::output_change:
		break;
	case event::volume_change://
		web_wsevent_send(configor::json({ {"type","volume"} }));
		break;
	case event::initiate:
		break;
	default:
		break;
	}
}

void webapp_thread(int port)
{
	web_app->port(port).multithreaded().run();
}
int web_uninit()
{
	web_app->stop();
	web_th.join();
	//delete web_app; //crash in release
	web_app = nullptr;
	return 0;
}

void web_wsevent_send(std::string event)
{
	if (web_wsc)
	{
		web_wsc->send_text(sutil::w2s(sutil::s2w(event, CP_ACP), CP_UTF8));
	}
}

void web_wsevent_send(configor::json j)
{
	if (web_wsc)
	{
		web_wsc->send_text(to_string(j));
	}
}
void web_open_browser(std::string url)
{
	const TCHAR szOperation[] = _T("open");
	HINSTANCE hRslt = ShellExecute(NULL, szOperation,
		sutil::s2w(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
}