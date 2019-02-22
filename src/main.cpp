// main.cpp
#include "http/server.h"


int main(int argv, char* argc[])
{
	http::Server server("127.0.0.1", 8080, nullptr);
	server.listen_and_serve();

	return 0;
}