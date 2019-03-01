// connection.h
#ifndef __IMGUP_HTTP_SERVER_H__
#define __IMGUP_HTTP_SERVER_H__
#include <memory>
#include <set>
#include <vector>
#include <map>
#include <boost/asio.hpp>

using namespace boost::asio;
using error_code = boost::system::error_code;


namespace http
{
	struct Request
	{
		std::string method;
		std::string path;
		std::map<std::string, std::string> headers;
		uint16_t http_major;
		uint16_t http_minor;
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

		static const std::string strEndOfLine;
		static const std::string strStatusGood;
		static const std::string strStatusBadRequest;
		static const std::string strStatusNotFound;
		static const std::string strStatusServerError;

		std::vector<std::string> headers;
		std::vector<char> body;
		Status status;
		void add_header(const char* name, const char* value);
		void prepare();
		std::vector<const_buffer> to_buffers();
	};

	class Server
	{
	public:
		Server(const Server&) = delete;
		const Server& operator= (const Server&) = delete;

		Server(const char* addr, uint16_t port, const char* doc_root);
		void add_handler(const char* path, std::function<void(const Request&, Response&)> handler);
		void listen_and_serve();
	
	private:
		struct Connection : public std::enable_shared_from_this<Connection>
		{
			Server* server;
			ip::tcp::socket socket;
			std::array<char, 8192> buffer;
			Request request;
			Response response;
			
			Connection(const Connection&) = delete;
			Connection& operator= (const Connection&) = delete;

			Connection(Server* srv, ip::tcp::socket soc) : server(srv), socket(std::move(soc)) {}
			~Connection();

			void start();
			void stop();

			void read_and_write();

			//error_code read_some(const char* buffer, int len);
			void write(Response& response);
		};

		void accept();
		void stop_connection(std::shared_ptr<Connection> c);

		io_service context;
		ip::tcp::endpoint endpoint;
		ip::tcp::acceptor acceptor;
		ip::tcp::socket socket;

		std::set<std::shared_ptr<Connection>> connection_pool;
		std::unordered_map<std::string, std::function<void(const Request&, Response&)>> handlers;
	};
	
} // namespace http
#endif // __IMGUP_HTTP_SERVER_H__