#pragma once
int web_init(int port = 10709);
int web_uninit();
void web_open_browser(std::string url);