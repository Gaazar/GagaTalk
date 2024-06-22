#pragma once
#include <asio.hpp>
class tcp_connection : public std::enable_shared_from_this<tcp_connection>
{
	typedef std::shared_ptr<tcp_connection> ptr;
	typedef asio::ip::tcp::endpoint endpoint_t;
	typedef asio::ip::tcp::socket socket_t;
	typedef asio::ip::address address_t;
	typedef asio::ip::tcp::resolver resolver_t;
	typedef asio::ip::tcp::resolver::results_type entiries_t;
	typedef asio::error_code error_t;

	asio::io_context& ioc;
	resolver_t resolver;
	socket_t sk_cmd;
	socket_t sk_voip;

public:
	static ptr connect(asio::io_context& io, std::string host, uint16_t port = 7970)
	{
		ptr p(new tcp_connection(io));
		p->_connect(host, port);
		return p;
	}

private:
	tcp_connection(asio::io_context& io) :ioc(io), resolver(ioc), sk_cmd(ioc), sk_voip(ioc)
	{
	};
	void _connect(std::string host, uint16_t port)
	{
		resolver.async_resolve(host, std::to_string(port),
			std::bind(&tcp_connection::resolved, shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2));
	}

	void resolved(error_t e, entiries_t entries)
	{
		std::cout << "resolve result: " << e << std::endl;
		for (auto& i : entries)
		{
			std::cout << i.endpoint().address().to_string() << "\n";
		}
		if (!e)
		{
			asio::async_connect(sk_cmd, entries, std::bind(&tcp_connection::connected, shared_from_this(), std::placeholders::_1));
		}
		return;
	}

	void connected(error_t e) //tcp connect result
	{
		std::cout << "connect result: " << e << std::endl;
		std::cout << "local endpoint: " << sk_cmd.local_endpoint().address().to_string() << ":" << sk_cmd.local_endpoint().port() << "\n";
		std::cout << "remote endpoint: " << sk_cmd.remote_endpoint().address().to_string() << ":" << sk_cmd.remote_endpoint().port() << std::endl;;

	}
	void on_handshake()
	{

	}
	void established() //handshake result
	{
	}
	void on_receive()
	{
	}

	void disconnected()
	{
	
	}
};