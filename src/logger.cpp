#include "logger.h"
#include <chrono>

void log_i(const char* module, const char* what)
{
	//timespec ts;
	//timespec_get(&ts, TIME_UTC);
	//char buff[100];
	//strftime(buff, sizeof buff, "%D %T", std::gmtime(&ts.tv_sec));
	//printf("Current time: %s.%09ld UTC\n", buff, ts.tv_nsec);
}
void log_w(const char* module, const char* what)
{

}
void log_e(const char* module, const char* what)
{

}