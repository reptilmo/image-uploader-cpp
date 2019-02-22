// connection.h
#ifndef __IMGUP_SERVER_H__
#define __IMGUP_SERVER_H__
#include <boost/asio.hpp>

using namespace boost;
using namespace boost::system;
using namespace boost::asio;

namespace http
{
	struct Header
	{
		std::string name;
		std::string value;
	};

	struct Request
	{
		std::string method;
		std::string path;
		uint16_t http_major;
		uint16_t http_minor;
		std::vector<Header> headers;
	};

	struct Response
	{
		enum class Status
		{
			Good,
			BadRequest,
			NotFound,
			ServerError
		};

		Status status;
		void add_header(const char* name, const char* value);
		void write_response_body(const unsigned char* data, std::size_t len);
		
	private:
		std::vector<Header> headers;

	};

	class Server
	{
	public:
		Server(const Server&) = delete;
		const Server& operator= (const Server&) = delete;

		Server(const char* addr, uint16_t port, const char* doc_root);
		void add_handler();
		void listen_and_serve();
	
	private:
		struct Connection
		{
			ip::tcp::socket socket;
			std::array<unsigned char, 8192> buffer;
			Server* server;

			Connection(const Connection&) = delete;
			Connection& operator= (const Connection&) = delete;

			Connection(ip::tcp::socket soc, Server* srv) : socket(std::move(soc)), server(srv) {}
			~Connection() {}

			void run();
			void read();
			void write();
		};

		void accept();

		io_service context;
		ip::tcp::endpoint endpoint;
		ip::tcp::acceptor acceptor;
		ip::tcp::socket socket;

		using ConnPtr = std::shared_ptr<Connection>;
		std::vector<ConnPtr> connection_pool;

		std::unordered_map<std::string, std::function<void(const Request&, Response&)>> handlers;
	};
	
} // namespace http
#endif // __IMGUP_SERVER_H__