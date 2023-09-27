#include "gt_defs.h"
#include "client.h"
#include <stdio.h>
#include <sstream>
#include <iostream>
#include <configor/json.hpp>

#pragma comment(lib,"opus.lib")
#define SESSION_ID 0x00FF8877 // 16746615

void print_usage_conf()
{
	printf("conf commands:\n");
	printf("q	: 离开设置\n");
	printf("ls	: 显示音频效果器列表\n");
	printf("rm <索引>	: 移除指定效果器\n");
	printf("d <索引>	: 禁用指定效果器\n");
	printf("e <索引>	: 启用指定效果器\n");
	printf("pu <效果器类型>	: 添加效果器(RNNDenoise,Compressor,Gain&Clamp)\n\tRNNDenoise: AI降噪\n\tCompressor: 压缩器，响度均衡\n\tGain&Clamp: 增益，削波\n");
	printf("m <索引> <参数名(括号里的英文)> <参数值>	: 修改效果器参数\n");
}
void print_usage()
{
	printf("commands:\n");
	printf("c <服务器地址> <昵称> [-p port]	: 连接服务器 官方服务器: gaga.fit\n");
	printf("j <频道ID>	: 切换频道.\n");
	printf("mm	: 闭麦.\n");
	printf("ss	: 静音.\n");
	printf("vm <总音量:0到100>	: 总音量\n");
	printf("vlb	: 听反.\n");
	printf("cvi <设备ID>	: 切换输入设备 设备ID为最上面列表的{...}.{.....}. 不能填输出设备的ID，不然死给你看。\n");
	printf("cvo <设备ID>	: 切换输出设备 设备ID为最上面列表的{...}.{.....}. 不能填输入设备的ID，不然死给你看。\n");
	printf("ls : 显示频道列表和在线用户.\n");
	printf("conf : 设置.\n");
	printf("v <用户代码 一串数字> <音量:0到100>	: ls 列表里面没有[]用户名左边的数字\n");
	//printf("下面都没实现\n");
	//printf("m	[client name|client id] <volume {0,100}>	: mute or unmute specified clients, show current mute state if only m.\n");


	printf("\n\n");
}
std::vector<std::string> path;


void print_head()
{
	for (auto& i : path)
		std::cout << i << ">";
}
void shell_conf()
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
	while (!terminated)
	{
		ch = getchar();
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
					printf("---------------------------------------\n索引:%d(%s)\n", n, i["enable"].as_bool() ? "已启用" : "已禁用");
					printf("\t名称: %s\n", i["name"].as_string().c_str());
					printf("\t类型: %s\n", t.c_str());
					printf("\t参数: \n");
					if (t == "RNNDenoise")
					{
						printf("\t\t无\n");
					}
					else if (t == "Compressor")
					{
						printf("\t\t门限(threshold): %fdb\n", args["threshold"].as_float());
						printf("\t\t比率(ratio): %f:1\n", args["ratio"].as_float());
						printf("\t\t启动时间(attack): %fms\n", args["attack"].as_float());
						printf("\t\t释抑时间(release): %fms\n", args["release"].as_float());
						printf("\t\t输出增益(gain): %fdb\n", args["gain"].as_float());
					}
					else if (t == "Gain&Clamp")
					{
						printf("\t\t增益(gain): %fdb\n", args["gain"].as_float());
						printf("\t\t波顶(clamp): %fdb\n", args["clamp"].as_float());

					}
					n++;
				}
			}
			else if (cmd[0] == "rm")
			{
				if (cmd.n_args() < 2)
				{
					printf("参数不足\n");
					goto tail;
				}
				auto i = stru64(cmd[1]);
				if (i < 0 || i >= j.size())
				{
					printf("没有该索引\n");
					goto tail;
				}
				j.erase(i);
				goto set_filter;
			}
			else if (cmd[0] == "pu")
			{
				if (cmd.n_args() < 2)
				{
					printf("参数不足\n");
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
				if (t == "RNNDenoise")
				{
					j.push_back(configor::json::parse(R"({"name": "name","type": "Gain&Clamp","args": {"gain": 3,"clamp": 0.0},"enable": true})"));
				}
				goto set_filter;
			}
			else if (cmd[0] == "d")
			{
				if (cmd.n_args() < 2)
				{
					printf("参数不足\n");
					goto tail;
				}
				auto i = stru64(cmd[1]);
				if (i < 0 || i >= j.size())
				{
					printf("没有该索引\n");
					goto tail;
				}
				j[i]["enable"] = false;
				goto set_filter;

			}
			else if (cmd[0] == "e")
			{
				if (cmd.n_args() < 2)
				{
					printf("参数不足\n");
					goto tail;
				}
				auto i = stru64(cmd[1]);
				if (i < 0 || i >= j.size())
				{
					printf("没有该索引\n");
					goto tail;
				}
				j[i]["enable"] = true;
				goto set_filter;

			}
			else if (cmd[0] == "m")
			{
				if (cmd.n_args() < 4)
				{
					printf("参数不足\n");
					goto tail;
				}
				auto i = stru64(cmd[1]);
				if (i < 0 || i >= j.size())
				{
					printf("没有该索引\n");
					goto tail;
				}
				if (!j[i]["args"].count(cmd[2]))
				{
					printf("没有该参数\n");
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
int shell_main()
{
	srand(time(nullptr));
	platform_init();
	client_init();
	print_usage();
	char ch;
	command_buffer cb;
	connection c;
	//int a = cb.append(t, sizeof(t));
	//a = cb.append(t1, sizeof(t1));
	//a = cb.append(t2, sizeof(t2));
	//ch = cb.parse(argv, 32);
	//for (int i = 0; i < ch; i++)
	//{
	//	printf("[%s]\n", argv[i]);
	//}
	print_head();
	command cmd;
	bool vlb_ = false;
	while (!terminated)
	{
		ch = getchar();
		if (cb.append(&ch, 1))
		{

			//int nn = cb.parse(cmd);

			//cmd.clear();
			//continue;
			cmd.clear();
			if (!cb.parse(cmd)) continue;
			if (cmd[0] == "ls")
			{
				if (c.status != connection::state::established) continue;
				for (auto& c : c.channels)
				{
					printf("[%u: %s]\n", c.first, c.second->name.c_str());
					for (auto& u : c.second->entities)
					{
						printf("\t%u: %s\n", u->suid, u->name.c_str());
					}
				}
			}
			else if (cmd[0] == "v")
			{
				if (cmd.n_args() < 3)
				{
					printf("参数不足\n");
					continue;
				}
				uint32_t suid = stru64(cmd[1]);
				if (!c.entities.count(suid))
				{
					printf("用户不存在\n");
					continue;
				}
				if (suid == c.suid)
				{
					printf("不能调自己\n");
					continue;
				}
				c.set_client_volume(suid, percent_to_db(stru64(cmd[2]) / 100.f));
				printf("OK\n");

			}
			else if (cmd[0] == "m")
			{
				if (cmd.n_args() < 2)
				{
					printf("参数不足\n");
					continue;
				}
				uint32_t suid = stru64(cmd[1]);
				if (!c.entities.count(suid))
				{
					printf("用户不存在\n");
					continue;
				}
				if (suid == c.suid)
				{
					printf("不能调自己\n");
					continue;
				}
				c.set_client_mute(suid, !c.get_client_mute(suid));
				printf("OK\n");

			}
			else if (cmd[0] == "c")
			{
				if (cmd.n_args() < 3) continue;
				if (c.status != connection::state::disconnect) continue;
				conf_set_username(cmd[2]);
				auto cr = c.connect(cmd[1].c_str(), 7970);
				printf("connect result: %d\n", cr);

			}
			else if (cmd[0] == "vlb")
			{
				vlb_ = !vlb_;
				if (vlb_)
					enable_voice_loopback();
				else
					disable_voice_loopback();
			}
			else if (cmd[0] == "cvi")
			{
				if (cmd.n_args() < 2)
				{
					plat_set_input_device("");
					conf_set_input_device(nullptr);
					printf("success set to default\n");
				}
				else if (plat_set_input_device(cmd[1]))
				{
					conf_set_input_device(&cmd[1]);
					printf("success\n");
				}
				else
					printf("failed\n");
			}
			else if (cmd[0] == "cvo")
			{
				if (cmd.n_args() < 2)
				{
					plat_set_output_device("");
					conf_set_output_device(nullptr);
					printf("success set to default\n");
				}
				else if (plat_set_output_device(cmd[1]))
				{
					conf_set_output_device(&cmd[1]);
					printf("success\n");
				}
				else
					printf("failed\n");

			}
			else if (cmd[0] == "mm")
			{
				bool m = !plat_get_global_mute();
				plat_set_global_mute(m);
				conf_set_global_mute(m);
				printf("%s\n", m ? "mute" : "not mute");
			}
			else if (cmd[0] == "ss")
			{
				bool s = !plat_get_global_silent();
				plat_set_global_silent(s);
				conf_set_global_silent(s);
				printf("%s\n", s ? "silent" : "not silent");
			}
			else if (cmd[0] == "j")
			{
				if (cmd.n_args() < 2) continue;
				c.join_channel(stru64(cmd[1]));
			}
			else if (cmd[0] == "vm")
			{
				if (cmd.n_args() < 2) continue;
				float vdb = percent_to_db(stru64(cmd[1]) / 100.f);
				plat_set_global_volume(vdb);
				conf_set_global_volume(vdb);
			}
			else if (cmd[0] == "conf")
			{
				shell_conf();
			}
			else if (cmd[0] == "dump")
			{
				int* nptr = nullptr;
				*nptr = 768;
			}
			else if (cmd[0] == "de")
			{
				for (auto i : c.entities)
				{
					printf("%s(%u)[%u]: vp=%p, Npak=%u\n", i.second->name.c_str(), i.second->suid,
						i.second->current_chid, i.second->playback, i.second->n_pak);
				}
			}
			print_head();
		}
	}

	return 0;
}