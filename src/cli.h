#pragma once
#include "gt_defs.h"
#include "pipe_stream.h"

struct shell_ctx
{
	std::vector<std::string> path;
	std::istream* in;
	std::ostream* out;
	void print_usage_conf();
	void print_usage_sapi();
	void print_usage();
	void print_head();
	void shell_conf();
	void shell_sapi();
	void shell_man();
	void shell_remote_control(suid_t dest);
	void shell_sub_netopt(command& cmd);
	void ls_dev();
	int start();
	shell_ctx(std::istream& i, std::ostream& o);
};
struct connection;
class conn_streamout : public std::streambuf
{
	connection* conn;
	suid_t dest;
public:
	conn_streamout(connection*, suid_t destination);
protected:
	int overflow(int c);
	std::streamsize xsputn(const char* s, std::streamsize size);
	int sync() override {
		return 0;
	}

};
struct remote_shell
{
	shell_ctx* ctx = nullptr;
	conn_streamout* bout = nullptr;
	pipe_buf bin;
	std::iostream* sout;
	std::iostream* sin;
	std::thread th_ctx;
	remote_shell(connection*, suid_t dest);
	~remote_shell();
	void input(std::string);
};