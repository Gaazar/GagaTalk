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
	int c;
	std::cin >> c;
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return 0;
}