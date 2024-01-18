#define CROW_STATIC_DIRECTORY "dist/"
#include <configor/json.hpp>
#include <crow.h>
#include <crow/middlewares/cors.h>
#include "web.h"
#include <filesystem>
#include "client.h"
void webapp_thread(int port);
void web_wsevent_send_raw(std::string event);
void web_wsevent_send(std::string event);
typedef crow::App<crow::CORSHandler> WebUI;
WebUI* web_app = nullptr;
std::thread web_th;
crow::websocket::connection* web_wsc_raw = nullptr;
crow::websocket::connection* web_wsc = nullptr;
int web_init(int port)
{
	web_app = new WebUI;
	auto& app = *web_app;
	auto& cors = app.get_middleware<crow::CORSHandler>();
	cors
		.global()
		.origin("https://gagarun.chat,http:127.0.0.1:10709");
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
		item["host"] = i.hostname;
		item["icon"] = i.icon;
		res.push_back(std::move(item));
	}
	response.write(res.dump());
	return response;
		});
	CROW_ROUTE(app, "/api/server/connect").methods("POST"_method)([]() {
		crow::response response;
	response.set_header("Content-Type", "application/json");

	return response;
		});
	CROW_ROUTE(app, "/api/server/disconnect").methods("POST"_method)([]() {
		crow::response response;
	response.set_header("Content-Type", "application/json");

	return response;
		});
	CROW_ROUTE(app, "/api/client/volume").methods("POST"_method)([]() {
		crow::response response;
	response.set_header("Content-Type", "application/json");

	return response;
		});
	auto& cfg_a = CROW_ROUTE(app, "/api/server/config/audio");
	cfg_a.methods("POST"_method)([]() {
		crow::response response;
	response.set_header("Content-Type", "application/json");
	return response;
		});

	cfg_a.methods("GET"_method)([](const crow::request& req) {
		/*
		query:	suid

		return:
		{
			master_volume: float,db
			suid:
			{
				volume: float,db
				mute: bool
				mute_type: ["server","client"]
			}
		}
		*/
		req.url_params.get("suid");
	crow::response response;
	response.set_header("Content-Type", "application/json");
	return response;
		});
	/*
	CROW_ROUTE(app, "/api/")([]() {
		crow::response response;
	response.set_header("Content-Type", "application/json");

		return response;
		});
	*/

	auto& wsrc = CROW_WEBSOCKET_ROUTE(app, "/api/raw");

	wsrc.onopen([&](crow::websocket::connection& conn)
		{
			if (web_wsc_raw)
			{
				auto wsc = web_wsc_raw;
				web_wsc_raw = nullptr;
				wsc->close();
				printf("wsraw connect\n");
			}
			else
			{
				printf("wsraw reconnect\n");
			}
	web_wsc_raw = &conn;

		});
	wsrc.onclose([&](crow::websocket::connection& conn, const std::string& reason)
		{
			if (web_wsc_raw == &conn)
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
	app.loglevel(crow::LogLevel::Warning);
	std::cout << "WebUI is running on http://127.0.0.1:10709" << std::endl;
	web_th = std::thread([=]() {
		webapp_thread(port);
		});
	return 0;
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

void web_wsevent_send_raw(std::string event)
{
	if (web_wsc_raw)
	{
		web_wsc_raw->send_text(event);
	}
}
void web_wsevent_send(std::string event)
{
	if (web_wsc)
	{
		web_wsc->send_text(event);
	}
}
