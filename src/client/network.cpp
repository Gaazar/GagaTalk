#include "client.h"
#include "network.h"
int init_network()
{
	return 0;
}
int main()
{
	asio::io_context ctx;
	auto conn = tcp_connection::connect(ctx, "localhost");
	ctx.run();
	system("pause");
	return 0;
}