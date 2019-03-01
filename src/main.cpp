// main.cpp
#include "http/server.h"
#include <fstream>

int main(int argv, char* argc[])
{
	http::Server server("127.0.0.1", 8080, nullptr);
	server.add_handler("/", [](const http::Request& req, http::Response& resp)
	{
		//FIXME: experimental
		std::ifstream html_file("/home/reptilmo/image-uploader-cpp/html/upload.html", std::ios::in);
		html_file.seekg (0, html_file.end);
		const int length = html_file.tellg();
		html_file.seekg (0, html_file.beg);

		resp.body.resize(length);
		html_file.read(resp.body.data(), resp.body.size());
		resp.status = http::Response::Status::Good;

	});

	server.listen_and_serve();
	return 0;
}
