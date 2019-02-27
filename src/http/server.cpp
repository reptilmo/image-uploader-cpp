// server.cpp
#include "server.h"
#include <memory>
#include <iostream>

namespace http
{

enum class ParserStatus
{
	Done,
	More,
	BadRequest
};

static ParserStatus parseRequest(char* buffer, int len, Request& request)
{
	char* start = buffer;
	int offset = 0;
	std::string::size_type sz;

	while (offset < len && start[offset] != ' ') ++offset;
	request.method.assign(start, offset);
	//NOTE: Only GET, POST and PUT are supported at this time
	if (request.method != "GET" && request.method != "POST" && request.method != "PUT")
		return ParserStatus::BadRequest;

	offset += 1;
	start = start + offset;
	len = len - offset;
	offset = 0;

	while (offset < len && start[offset] != ' ') ++offset;
	request.path.assign(start, offset);

	offset += 1;
	start = start + offset;
	len = len - offset;
	offset = 0;

	while (offset < len && start[offset] != '/') ++offset;
	if (memcmp(start, "HTTP", offset) != 0)
		return ParserStatus::BadRequest;

	offset += 1;
	start = start + offset;
	len = len - offset;
	offset = 0;

	while (offset < len && start[offset] != '.') ++offset;
	std::string str(start, offset);
	request.http_major = std::stoi(str, &sz);

	offset += 1;
	start = start + offset;
	len = len - offset;
	offset = 0;

	while (offset < len && start[offset] != '\r' && start[offset] != '\n') ++offset;
	str.assign(start, offset);
	request.http_minor = std::stoi(str, &sz);

	offset += 2;
	start = start + offset;
	len = len - offset;
	offset = 0;

	if (start[offset] == '\r' || start[offset] == '\n')
		return ParserStatus::BadRequest;

	while (len)
	{
		if (start[0] == '\r' || start[0] == '\n')
			break;

		char* start_header_name = start;
		while (start[offset] != ':') ++offset;
		int len_header_name = offset;

		offset += 1;
		start = start + offset;
		len = len - offset;
		offset = 0;

		char* start_header_value = start;
		while (start[offset] != '\r' && start[offset] != '\n') ++offset;
		int len_header_value = offset;

		request.headers.emplace_back(start_header_name,
			len_header_name, start_header_value, len_header_value);

		offset += 2;
		start = start + offset;
		len = len - offset;
		offset = 0;
	}

	return ParserStatus::Done;
}

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
			auto c = std::make_shared<Connection>(this, std::move(socket));
			c->start();
			connection_pool.insert(c);
		}

		accept();
	});
}


std::size_t print_line(char* buffer, std::size_t len)
{
	char line[256] = {0};
	std::size_t n = 0;

	while (n < 256 && n < len && buffer[n] != '\r') ++n;
	std::memcpy(line, buffer, n);
	std::printf("%s\n", line);

	return n + 2; //  '\r\n'
}

void Server::Connection::start() 
{
	read();
}

void Server::Connection::stop()
{
	socket.close();
}

void Server::Connection::read()
{
	socket.async_read_some(boost::asio::buffer(buffer, buffer.size()), [this](error_code ec, std::size_t size)
	{
		if (!ec)
		{
			try
			{
				switch (parseRequest(buffer.data(), size, request))
				{
					case ParserStatus::Done:
					{
						auto iter = server->handlers.find(request.path);
						if (iter != server->handlers.end())
							iter->second(request, response);

					}
					case ParserStatus::More:
					default:
					{
						server->stop_connection(shared_from_this());
						return;
					}
				}
			}
			catch (std::exception& e)
			{
				std::printf("%s\n", e.what());
			}
			catch (...)
			{
				std::printf("error\n");
			}
		}
		else
		{
			write();
			server->stop_connection(shared_from_this());
		}
	});
}

void Server::Connection::write()
{

}

void Server::stop_connection(std::shared_ptr<Connection> c)
{
	connection_pool.erase(c);
	c->stop();
}

} // namespace http
