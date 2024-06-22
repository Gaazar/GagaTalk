#include "../gt_defs.h"
#include "client.h"
#include <stdio.h>
#include <sstream>
#include <iostream>
#include <configor/json.hpp>
#include "web.h"
#include "events.h"
#include "../logger.h"
#include <fmt/core.h>
#include <fmt/format.h>
#include "../cli.h"

#pragma comment(lib,"opus.lib")
#define SESSION_ID 0x00FF8877 // 16746615

void shell_ctx::print_usage_conf()
{
	*out << ("conf commands:\n");
	*out << ("q	: 离开设置\n");
	*out << ("ls	: 显示音频效果器列表\n");
	*out << ("rm <索引>	: 移除指定效果器\n");
	*out << ("d <索引>	: 禁用指定效果器\n");
	*out << ("e <索引>	: 启用指定效果器\n");
	*out << ("pu <效果器类型>	: 添加效果器(RNNDenoise,Compressor,Gain&Clamp)\n\tRNNDenoise: AI降噪\n\tCompressor: 压缩器，响度均衡\n\tGain&Clamp: 增益，削波\n");
	*out << ("m <索引> <参数名(括号里的英文)> <参数值>	: 修改效果器参数\n");
}
void shell_ctx::print_usage_sapi()
{
	*out << ("sapi commands:\n");
	*out << ("q	: 离开设置\n");
	*out << ("v <音量>	: 设置音量0-100\n");
	*out << ("r <语速>	: 设置语速0-20\n");
	*out << ("d	: 禁用朗读\n");
	*out << ("e	: 启用朗读\n");
	*out << ("<测试朗读文本>	: 朗读自定义文本\n");

}
void shell_ctx::print_usage()
{
	*out << ("commands:\n");
	*out << ("c <服务器地址> <昵称> [-p port]	: 连接服务器 官方服务器: gaga.fit\n");
	*out << ("j <频道ID>	: 切换频道.\n");
	*out << ("mm	: 闭麦.\n");
	*out << ("ss	: 静音.\n");
	*out << ("vm <总音量:0到100>	: 总音量\n");
	*out << ("vlb	: 听反.\n");
	*out << ("cvi <设备ID>	: 切换输入设备 设备ID为最上面列表的{...}.{.....}. 不能填输出设备的ID，不然死给你看。\n");
	*out << ("cvo <设备ID>	: 切换输出设备 设备ID为最上面列表的{...}.{.....}. 不能填输入设备的ID，不然死给你看。\n");
	*out << ("ls : 显示频道列表和在线用户.\n");
	*out << ("v <用户代码 一串数字> <音量:0到100>	: ls 列表里面没有[]用户名左边的数字\n");
	*out << ("conf : 设置.\n");
	*out << ("sapi : 朗读设置.\n");
	*out << ("exit : 退出程序.\n");
	//printf("下面都没实现\n");
	//printf("m	[client name|client id] <volume {0,100}>	: mute or unmute specified clients, show current mute state if only m.\n");


	printf("\n\n");
}
void shell_ctx::print_head()
{
	for (auto& i : path)
		*out << i << ">";
}
void shell_ctx::shell_conf()
{

	command_buffer cb;
	command cmd;
	char ch;
	path.push_back("conf");
	print_usage_conf();
	print_head();
	std::string filter;
	configor::json j;
	conf_get_filter(filter);
	j = configor::json::parse(filter);
	enable_voice_loopback();
	while (!discard)
	{
		in->get(ch);
		if (cb.append(&ch, 1))
		{

			//int nn = cb.parse(cmd);

			//cmd.clear();
			//continue;
			cmd.clear();
			if (!cb.parse(cmd)) continue;
			if (cmd[0] == "q")
			{
				path.pop_back();
				disable_voice_loopback();
				return;
			}
			else if (cmd[0] == "ls")
			{
				int n = 0;
				for (auto i : j)
				{
					auto t = i["type"].as_string();
					auto args = i["args"];
					*out << fmt::format("---------------------------------------\n索引:{}({})\n", n, i["enable"].as_bool() ? "已启用" : "已禁用");
					*out << fmt::format("\t名称: {}\n", i["name"].as_string().c_str());
					*out << fmt::format("\t类型: {}\n", t.c_str());
					*out << ("\t参数: \n");
					if (t == "RNNDenoise")
					{
						*out << ("\t\t无\n");
					}
					else if (t == "Compressor")
					{
						*out << fmt::format("\t\t门限(threshold): {}db\n", args["threshold"].as_float());
						*out << fmt::format("\t\t比率(ratio): {}:1\n", args["ratio"].as_float());
						*out << fmt::format("\t\t启动时间(attack): {}ms\n", args["attack"].as_float());
						*out << fmt::format("\t\t释抑时间(release): {}ms\n", args["release"].as_float());
						*out << fmt::format("\t\t输出增益(gain): {}db\n", args["gain"].as_float());
					}
					else if (t == "Gain&Clamp")
					{
						*out << fmt::format("\t\t增益(gain): {}db\n", args["gain"].as_float());
						*out << fmt::format("\t\t波顶(clamp): {}db\n", args["clamp"].as_float());

					}
					n++;
				}
			}
			else if (cmd[0] == "rm")
			{
				if (cmd.n_args() < 2)
				{
					*out << ("参数不足\n");
					goto tail;
				}
				auto i = stru64(cmd[1]);
				if (i < 0 || i >= j.size())
				{
					*out << ("没有该索引\n");
					goto tail;
				}
				j.erase(i);
				goto set_filter;
			}
			else if (cmd[0] == "pu")
			{
				if (cmd.n_args() < 2)
				{
					*out << ("参数不足\n");
					goto tail;
				}
				std::string t = cmd[1];
				if (t == "RNNDenoise")
				{
					j.push_back(configor::json::parse(R"({"name":"name","type":"RNNDenoise","args":{}})"));
				}
				if (t == "Compressor")
				{
					j.push_back(configor::json::parse(R"({"name": "name","type": "Compressor","args": {"ratio": 10,"threshold": -18.0,"attack": 6,"release": 60,"gain": 0},"enable": true})"));
				}
				if (t == "Gain&Clamp")
				{
					j.push_back(configor::json::parse(R"({"name": "name","type": "Gain&Clamp","args": {"gain": 3,"clamp": 0.0},"enable": true})"));
				}
				goto set_filter;
			}
			else if (cmd[0] == "d")
			{
				if (cmd.n_args() < 2)
				{
					*out << ("参数不足\n");
					goto tail;
				}
				auto i = stru64(cmd[1]);
				if (i < 0 || i >= j.size())
				{
					*out << ("没有该索引\n");
					goto tail;
				}
				j[i]["enable"] = false;
				goto set_filter;

			}
			else if (cmd[0] == "e")
			{
				if (cmd.n_args() < 2)
				{
					*out << ("参数不足\n");
					goto tail;
				}
				auto i = stru64(cmd[1]);
				if (i < 0 || i >= j.size())
				{
					*out << ("没有该索引\n");
					goto tail;
				}
				j[i]["enable"] = true;
				goto set_filter;

			}
			else if (cmd[0] == "m")
			{
				if (cmd.n_args() < 4)
				{
					*out << ("参数不足\n");
					goto tail;
				}
				auto i = stru64(cmd[1]);
				if (i < 0 || i >= j.size())
				{
					*out << ("没有该索引\n");
					goto tail;
				}
				if (!j[i]["args"].count(cmd[2]))
				{
					*out << ("没有该参数\n");
					goto tail;
				}
				j[i]["args"][cmd[2]] = std::strtof(cmd[3].c_str(), nullptr);
				goto set_filter;
			}
			goto tail;
		set_filter:
			plat_set_filter(j.dump());
			conf_set_filter(j.dump());
		tail:
			print_head();

		}
	}
}
void shell_ctx::shell_sapi()
{
	command_buffer cb;
	command cmd;
	char ch;
	path.push_back("sapi");
	print_usage_sapi();
	print_head();
	while (!discard)
	{
		in->get(ch);
		if (cb.append(&ch, 1))
		{

			//int nn = cb.parse(cmd);

			//cmd.clear();
			//continue;
			cmd.clear();
			print_head();
			if (!cb.parse(cmd)) continue;
			if (cmd[0] == "q")
			{
				path.pop_back();
				conf_set_sapi(sapi_get_profile());
				*out << ("\n");
				return;
			}
			if (cmd[0] == "v")
			{
				if (cmd.n_args() < 2) continue;
				int v = stru64(cmd[1]);
				if (v < 0) v = 0;
				if (v > 100) v = 100;
				sapi_set_volume(v);
			}
			else if (cmd[0] == "r")
			{
				if (cmd.n_args() < 2) continue;
				int v = stru64(cmd[1]) - 10;
				if (v < -10) v = -10;
				if (v > 10) v = 10;
				sapi_set_rate(v);
			}
			else if (cmd[0] == "e")
			{
				sapi_enable();
			}
			else if (cmd[0] == "d")
			{
				sapi_disable();
			}
			else
			{
				sapi_say(cmd.str());
			}

		}
	}
}
void shell_ctx::shell_man()
{
	command_buffer cb;
	command cmd;
	char ch;
	path.push_back("man");
	print_head();
	while (!discard)
	{
		in->get(ch);
		if (cb.append(&ch, 1))
		{
			cmd.clear();
			print_head();
			if (!cb.parse(cmd)) continue;
			if (cmd[0] == "q")
			{
				path.pop_back();
				*out << ("\n");
				return;
			}
			else
			{
				auto c = client_get_current_server();
				auto s = "man " + cmd.str() + "\n";
				c->send_command(s.c_str(), s.length());
			}
		}
	}
}
void shell_ctx::shell_remote_control(suid_t dest)
{
	command_buffer cb;
	command cmd;
	char ch;
	path.push_back("remote");
	print_head();
	auto c = client_get_current_server();
	c->send_command(fmt::format("rdc open {}\n", dest));
	while (!discard)
	{
		in->get(ch);
		if (cb.append(&ch, 1))
		{
			cmd.clear();
			print_head();
			if (!cb.parse(cmd)) continue;
			auto s = fmt::format("rdd {} -i\n", escape(cmd.str() + "\n"));
			c->send_command(s.c_str(), s.length());
			if (cmd[0] == "exit")
			{
				path.pop_back();
				*out << ("\n");
				break;
			}
		}
	}
	c->send_command(fmt::format("rdc close {}\n", dest));
}
void shell_ctx::shell_sub_netopt(command& cmd)
{
	auto c = client_get_current_server();
	if (cmd.n_args() > 0)
	{
		if (cmd[0] == "pakf")
		{
			if (cmd.n_args() < 2) return;
			int ms = stru64(cmd[1]);
			if (ms != 10 && ms % 20 != 0 || ms < 10 || ms > 120) return;
			c->set_netopt_paksz(ms * 48);
			*out << ("OK\n");
		}
	}
	else
	{
		*out << ("pakf <10|20|40|60|80|100|120> 数据包帧长\n");
		*out << ("buff <1-64> 缓冲长度，数字越大延迟越高（默认16）\n");
	}

}
void shell_ctx::ls_dev()
{
	std::vector<audio_device> devs;
	plat_enum_input_device(devs);
	*out << ("输入设备：\n");
	for (auto& i : devs)
	{
		*out << fmt::format("\t{}: {}\n", i.name.c_str(), i.id.c_str());
	}
	plat_enum_output_device(devs);
	*out << ("输出设备：\n");
	for (auto& i : devs)
	{
		*out << fmt::format("\t{}: {}\n", i.name.c_str(), i.id.c_str());
	}

}
int shell_ctx::start()
{
	ls_dev();
	print_usage();
	print_head();
	command cmd;
	command_buffer cb;
	char ch;
	while (!discard)
	{
		in->get(ch);
		//ch = getchar();
		if (cb.append(&ch, 1))
		{

			//int nn = cb.parse(cmd);

			//cmd.clear();
			//continue;
			cmd.clear();
			if (!cb.parse(cmd)) continue;
			auto c = client_get_current_server();
			if (cmd[0] == "vlb")
			{
				toggle_voice_loopback();
			}
			else if (cmd[0] == "cvi")
			{
				if (cmd.n_args() < 2)
				{
					plat_set_input_device("");
					conf_set_input_device(nullptr);
				}
				else if (plat_set_input_device(cmd[1]))
				{
					conf_set_input_device(&cmd[1]);
				}
				else
					*out << ("操作失败\n");
			}
			else if (cmd[0] == "cvo")
			{
				if (cmd.n_args() < 2)
				{
					plat_set_output_device("");
					conf_set_output_device(nullptr);
				}
				else if (plat_set_output_device(cmd[1]))
				{
					conf_set_output_device(&cmd[1]);
				}
				else
					*out << ("操作失败\n");

			}
			else if (cmd[0] == "vio")
			{
				*out << fmt::format("输入：{}\n输出：{}\n", plat_get_input_device().c_str(), plat_get_output_device().c_str());
			}
			else if (cmd[0] == "mm")
			{
				bool m = !plat_get_global_mute();
				client_set_global_mute(m);
			}
			else if (cmd[0] == "ss")
			{
				bool s = !plat_get_global_silent();
				client_set_global_silent(s);
			}
			else if (cmd[0] == "vm")
			{
				if (cmd.n_args() < 2) continue;
				float vdb = percent_to_db(stru64(cmd[1]) / 100.f);
				plat_set_global_volume(vdb);
				conf_set_global_volume(vdb);
			}
			else if (cmd[0] == "vv")
			{
				float db;
				conf_get_global_volume(db);
				*out << fmt::format("master volume: {}db, {}%\n", db, db_to_percent(db) * 100);
			}
			else if (cmd[0] == "conf")
			{
				shell_conf();
			}
			else if (cmd[0] == "sapi")
			{
				shell_sapi();
			}
			else if (cmd[0] == "ld")
			{
				ls_dev();
			}
			else if (cmd[0] == "ver")
			{
				*out << "build seq:" << BUILD_SEQ << "\n";
			}
			else if (cmd[0] == "dump")
			{
				int* nptr = nullptr;
				*nptr = 768;
			}
			else if (cmd[0] == "exit")
			{
				break;
			}
			else if (cmd[0] == "c")
			{
				if (cmd.n_args() < 3) continue;
				if (c && c->host + ":7970" != cmd[1] + ":7970") client_disconnect(c);
				conf_set_username(cmd[2]);
				auto cr = client_connect(cmd[1].c_str(), 7970, &c);
				*out << fmt::format("connect result: {}\n", cr);

			}
			else if (!c)
			{
				*out << ("需要先连接服务器\n");
				print_head();
			}
			else if (c)
			{
				if (cmd[0] == "ls")
				{
					if (c->status != connection::state::established) continue;
					for (auto& c : c->channels)
					{
						*out << fmt::format("[{}: {}]\n", c.first, c.second->name.c_str());
						for (auto& u : c.second->entities)
						{
							*out << fmt::format("\t{}: {}\n", u->suid, u->name.c_str());
						}
					}
				}
				else if (cmd[0] == "v")
				{
					if (cmd.n_args() < 3)
					{
						*out << ("参数不足\n");
						continue;
					}
					uint32_t suid = stru64(cmd[1]);
					if (!c->entities.count(suid))
					{
						*out << ("用户不存在\n");
						continue;
					}
					if (suid == c->suid)
					{
						*out << ("不能调自己\n");
						continue;
					}
					c->set_client_volume(suid, percent_to_db(stru64(cmd[2]) / 100.f));

				}
				else if (cmd[0] == "m")
				{
					if (cmd.n_args() < 2)
					{
						*out << ("参数不足\n");
						continue;
					}
					uint32_t suid = stru64(cmd[1]);
					if (!c->entities.count(suid))
					{
						*out << ("用户不存在\n");
						continue;
					}
					if (suid == c->suid)
					{
						*out << ("不能调自己\n");
						continue;
					}
					c->set_client_mute(suid, !c->get_client_mute(suid));

				}
				else if (cmd[0] == "j")
				{
					if (cmd.n_args() < 2) continue;
					c->join_channel(stru64(cmd[1]));
				}
				else if (cmd[0] == "de")
				{
					for (auto i : c->entities)
					{
						*out << fmt::format("{}({})[{}]:\tvp={}\n\tn_pak={},\tnb_pak={}\n", i.second->name.c_str(), i.second->suid, i.second->current_chid, fmt::ptr(i.second->playback),
							i.second->debug_state.n_pak_recv, i.second->debug_state.nb_pak_recv);
					}
					*out << ("\nlocal:\n");
					*out << fmt::format("\tn_pak_sent={},\tnb_pak_sent={}\n\tb_null_pb={},\tn_pak_err_enc={}\n",
						c->debug_state.n_pak_sent, c->debug_state.nb_pak_sent, c->debug_state.n_null_pb, c->debug_state.n_pak_err_enc);
					*out << "\nremote debug:\n"
						<< "shell=" << c->debug_state.shell << "\n";
				}
				else if (cmd[0] == "man")
				{
					if (cmd.n_args() == 1)
					{
						shell_man();
					}
					else
					{
						auto s = cmd.str() + "\n";
						c->send_command(s.c_str(), s.length());
					}
				}
				else if (cmd[0] == "play")
				{

				}
				else if (cmd[0] == "remote")
				{
					if (cmd.n_args() < 2)
						continue;
					shell_remote_control(stru64(cmd[1]));

				}
				else if (cmd[0] == "netopt")
				{
					cmd.remove_head();
					shell_sub_netopt(cmd);
				}

			}
			print_head();
		}
	}
	return 0;
}
shell_ctx::shell_ctx(std::istream& i, std::ostream& o)
{
	in = &i;
	out = &o;
}

conn_streamout::conn_streamout(connection* c, suid_t d) : conn(c), dest(d)
{

}
int conn_streamout::overflow(int c)
{
	if (c == EOF || !conn)
		return 0;
	char b = c;
	conn->send_command(fmt::format("rdd {} -o\n", esc_quote(&b, 1)));
	return c;
}
std::streamsize conn_streamout::xsputn(const char* s, std::streamsize size)
{
	if (!conn) return 0;
	conn->send_command(fmt::format("rdd {} -o\n", esc_quote(s, size)));
	return size;
}

remote_shell::remote_shell(connection* c, suid_t d)
{
	bout = new conn_streamout(c, d);
	sout = new std::iostream(bout);
	sin = new std::iostream(&bin);
	ctx = new shell_ctx(*sin, *sout);
	th_ctx = std::thread([this]()
		{
			ctx->start();
		});
}
void remote_shell::input(std::string cmd)
{
	*sin << cmd;
}

remote_shell::~remote_shell()
{
	*sin << "\nexit\n";
	th_ctx.join();
	delete sin;
	delete sout;
	delete bout;
	delete ctx;
}



void shell_event_client(event t, void* data)
{
	switch (t)
	{
	case event::join:
		break;
	case event::left:
		break;
	case event::mute:
		printf("闭麦：%s\n", *(bool*)data ? "是" : "否");
		break;
	case event::silent:
		printf("静音：%s\n", *(bool*)data ? "是" : "否");
		break;
	case event::auth:
		break;
	case event::input_change:
		if (data)
		{
			std::vector<audio_device> d;
			plat_enum_input_device(d);
			for (auto& i : d)
			{
				if (i.id == (char*)data)
					printf("输入设备切换为：%s\n", i.name.c_str());
			}
		}
		else
		{
			printf("输入设备切换为：默认\n");
		}
		break;
	case event::output_change:
		if (data)
		{
			std::vector<audio_device> d;
			plat_enum_output_device(d);
			for (auto& i : d)
			{
				if (i.id == (char*)data)
					printf("输出设备切换为：%s\n", i.name.c_str());
			}
		}
		else
		{
			printf("输出设备切换为：默认\n");
		}
		break;
	case event::volume_change:
		printf("总音量：%f\n", *(float*)data);
		break;
		break;
	default:
		break;
	}
}
void shell_event_entity(event t, entity* e)
{
	switch (t)
	{
	case event::join:
		break;
	case event::left:
		break;
	case event::mute:
	{
		bool b = e->entity_state.mute || e->entity_state.man_mute;
		if (b)
		{
			printf("用户%s(%u)闭麦：是(", e->name.c_str(), e->suid);
			if (e->entity_state.mute)
				printf("主动 ");
			if (e->entity_state.man_mute)
				printf("服务器 ");
			printf(")\n");
		}
		else
		{
			printf("用户%s(%u)闭麦：否\n", e->name.c_str(), e->suid);

		}
		break;
	}
	case event::silent:
	{
		bool b = e->entity_state.silent || e->entity_state.man_silent || e->get_mute();
		if (b)
		{
			printf("用户%s(%u)静音：是(", e->name.c_str(), e->suid);
			if (e->get_mute())
				printf("本地 ");
			if (e->entity_state.silent)
				printf("主动 ");
			if (e->entity_state.man_silent)
				printf("服务器 ");
			printf(")\n");
		}
		else
		{
			printf("用户%s(%u)静音：否\n", e->name.c_str(), e->suid);

		}
		break;
	}
	case event::auth:
		break;
	case event::input_change:
		break;
	case event::output_change:
		break;
	case event::volume_change:
		if (e->playback)
			printf("用户%s(%u)的音量：%f\n", e->name.c_str(), e->suid, e->get_volume());
		break;
	default:
		break;
	}
}
void shell_event_server(event t, connection* c)
{
	switch (t)
	{
	case event::join:
		break;
	case event::left:
		break;
	case event::mute:
		break;
	case event::silent:
		break;
	case event::auth:
		break;
	case event::fail:
		printf(fmt::format("无法连接至服务器: {}:{}\n", c->host, c->port).c_str());
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
int shell_main()
{
	srand(time(nullptr));
	client_init();
	shell_ctx sh(std::cin, std::cout);
	e_client += shell_event_client;
	e_entity += shell_event_entity;
	e_server += shell_event_server;
	client_check_update();
	sh.start();
	client_uninit();

	return 0;
}
