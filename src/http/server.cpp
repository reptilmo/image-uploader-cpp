// server.cpp
#include "server.h"
#include <memory>
#include <iostream>
#include <tuple>

namespace http
{

enum class ParserStatus
{
	Done,
	More,
	BadRequest
};

static std::tuple<ParserStatus, char*, int> parseRequest(char* buffer, int len, Request& request)
{
	char* start = buffer;
	int offset = 0;
	std::string::size_type sz;

	while (offset < len && start[offset] != ' ') ++offset;
	request.method.assign(start, offset);
	//NOTE: Only GET, POST and PUT are supported at this time
	if (request.method != "GET" && request.method != "POST" && request.method != "PUT")
		return std::make_tuple(ParserStatus::BadRequest, nullptr, 0);

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
		return std::make_tuple(ParserStatus::BadRequest, nullptr, 0);

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
		return std::make_tuple(ParserStatus::BadRequest, nullptr, 0);

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

		request.headers.emplace(std::string(start_header_name, len_header_name),
			std::string(start_header_value, len_header_value));

		offset += 2;
		start = start + offset;
		len = len - offset;
		offset = 0;
	}

	return std::make_tuple(ParserStatus::Done, nullptr, 0);
}

const std::string Response::strEndOfLine = "\r\n";
const std::string Response::strStatusGood = "HTTP/1.0 200 OK\r\n";
const std::string Response::strStatusBadRequest = "HTTP/1.0 400 Bad Request\r\n";
const std::string Response::strStatusNotFound = "HTTP/1.0 404 Not Found\r\n";
const std::string Response::strStatusServerError = "HTTP/1.0 500 Internal Server Error\r\n";

void Response::add_header(const char* name, const char* value)
{
	std::string header(name);
	header.append(": ");
	header.append(value);
	header.append("\r\n");

	headers.push_back(std::move(header));
}

void Response::prepare()
{
	if (status == Status::Good || status == Status::NotFound)
	{
		std::string sz(std::to_string(body.size()));
		add_header("Content-Length", sz.c_str());
	}
	else
	{
		body.clear();
		add_header("Content-Length", "0");
	}

	add_header("Content-Type", "text/html");
}

std::vector<const_buffer> Response::to_buffers()
{
	std::vector<boost::asio::const_buffer> buffers;
	switch (status)
	{
		case Status::Good:
			buffers.push_back(buffer(strStatusGood));
			break;
		case Status::BadRequest:
			buffers.push_back(buffer(strStatusBadRequest));
			break;
		case Status::NotFound:
			buffers.push_back(buffer(strStatusNotFound));
			break;
		case Status::ServerError:
			buffers.push_back(buffer(strStatusServerError));
			break;
		default:
			buffers.push_back(buffer(strStatusServerError));
			break;
	}

	for (auto& h : headers)
		buffers.push_back(buffer(h));

	buffers.push_back(buffer(strEndOfLine));
	buffers.push_back(buffer(body.data(), body.size()));
	return buffers;
}

Server::Server(const char* addr, uint16_t port, const char* doc_root)
: context()
, endpoint(ip::address::from_string(addr), port)
, acceptor(context)
, socket(context)
{
}

void Server::add_handler(const char* path, std::function<void(const Request&, Response&)> handler)
{
	handlers.emplace(path, handler);
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
			connection_pool.insert(c);
			c->start();
		}

		accept();
	});
}

Server::Connection::~Connection()
{
	//socket.close();
}

void Server::Connection::start() 
{
	read_and_write();
}

void Server::Connection::stop()
{
	socket.close();
}

void Server::Connection::read_and_write()
{
	auto self(shared_from_this());
	socket.async_read_some(boost::asio::buffer(buffer, buffer.size()), [this, self](error_code ec, std::size_t sz)
	{
		if (!ec) 
		{
			auto [status, next_buffer_start, next_buffer_size] = parseRequest(buffer.data(), sz, request);
			switch (status) 
			{
				case ParserStatus::Done:
				{
					auto iter = server->handlers.find(request.path);
					if (iter != server->handlers.end())
					{
						try
						{
							iter->second(request, response);
						}
						catch (std::exception& e)
						{
							response.status = Response::Status::ServerError;
						}
						catch (...)
						{
							response.status = Response::Status::ServerError;
						}
					}
					else
					{
						response.status = Response::Status::NotFound;
					}

					write(response);
					return;
				}
				case ParserStatus::More:
				{
					//FIXME: do some more reading here or allow user to continue reading from request in the handler
					(next_buffer_start);
					(next_buffer_size);

					return;
				}
				case ParserStatus::BadRequest:
				default:
				{
					response.status = Response::Status::BadRequest;
					write(response);
					return;
				}
			}
		}
		else
		{
			response.status = Response::Status::ServerError;
			write(response);
		}
	});
}

void Server::Connection::write(Response& response)
{
	auto self(shared_from_this());
	response.prepare();
	boost::asio::async_write(socket, response.to_buffers(), [this, self](error_code ec, std::size_t sz)
	{
		if (!ec)
		{
			error_code ignore;
			socket.shutdown(ip::tcp::socket::shutdown_both, ignore);
		}

	});
}

void Server::stop_connection(std::shared_ptr<Connection> c)
{
	{
		//FIXME: lock if running in threads
		connection_pool.erase(c);
	}

	c->stop();
}

} // namespace http
