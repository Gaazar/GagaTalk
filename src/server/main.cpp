#include "db.h"
#include "server.h"
#include <iostream>
int main(int argc, char** argv)
{
	setlocale(LC_ALL, "zh-CN");
	std::cout << "GagaTalk Server\n\tAPI port 7970\n\tVOIP port 17970\n";
	srand(time(nullptr));
	socket_init();
	instance s;
	s.start();
	char ch;
	command_buffer cb;
	//int a = cb.append(t, sizeof(t));
	//a = cb.append(t1, sizeof(t1));
	//a = cb.append(t2, sizeof(t2));
	//ch = cb.parse(argv, 32);
	//for (int i = 0; i < ch; i++)
	//{
	//	printf("[%s]\n", argv[i]);
	//}
	command cmd;
	bool vlb_ = false;
	while (!terminated)
	{
		ch = getchar();
		if (ch == -1)
			break;
		if (cb.append(&ch, 1))
		{

			//int nn = cb.parse(cmd);

			//cmd.clear();
			//continue;
			cmd.clear();
			if (!cb.parse(cmd)) continue;
			if (cmd == "q")
				terminated = true;
			else
				s.on_man_cmd(cmd, nullptr);

		}
	}
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return 0;
}