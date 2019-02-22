// server.cpp
#include "server.h"
#include <memory>

namespace http
{

Server::Server(const char* addr, uint16_t port, const char* doc_root)
: context()
, endpoint(ip::address::from_string(addr), port)
, acceptor(context)
, socket(context)
{
}

void Server::listen_and_serve()
{
	acceptor.open(endpoint.protocol());
	acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor.bind(endpoint);
	acceptor.listen();
	accept();
	context.run();
}

void Server::accept()
{
	acceptor.async_accept(socket, [this](error_code ec)
	{
		if (!acceptor.is_open())
			return;

		if (!ec)
		{
			auto c = std::make_shared<Connection>(std::move(socket), this /*, handler*/ );
			c->run();
			connection_pool.push_back(c);
		}

		accept();
	});
}


std::size_t print_line(unsigned char* buffer, std::size_t len)
{
	char line[256] = {0};
	std::size_t n = 0;

	while (n < 256 && n < len && buffer[n] != '\r') ++n;
	std::memcpy(line, buffer, n);
	std::printf("%s\n", line);

	return n + 2; //  '\r\n'
}

void Server::Connection::run() 
{
	read();
	write();
}

void Server::Connection::read()
{
	socket.async_read_some(boost::asio::buffer(buffer, buffer.size()), [this](error_code ec, std::size_t size)
	{
		if (!ec)
		{
			std::size_t read = 0;
			while (read < size)
				read += print_line(&buffer[read], size - read);
		}
		else
		{

		}
	});
}

void Server::Connection::write()
{

}

} // namespace http
