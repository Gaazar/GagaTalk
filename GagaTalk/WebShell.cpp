#define CROW_STATIC_DIRECTORY "dist/"
#include <configor/json.hpp>
#include <crow.h>
#include "web.h"
#include <filesystem>
#include "client.h"
void webapp_thread(int port);
crow::SimpleApp* web_app = nullptr;
std::thread web_th;
int web_init(int port)
{
	web_app = new crow::SimpleApp();
	crow::SimpleApp& app = *web_app;
	CROW_ROUTE(app, "/")([]() {
		crow::response response;
	response.set_static_file_info("./dist/index.html");

	return response;
		});

	CROW_ROUTE(app, "/api/servers/")([]() {
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
	CROW_ROUTE(app, "/api/server/join/")([]() {
		crow::response response;

	return response;
		});
	/*
	CROW_ROUTE(app, "/api/")([]() {
		crow::response response;

		return response;
		});
	*/


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