
#include "Debug.h"
#include "TcpListener.h"

Debug logger;


int main()
{
	LOG("main started");

	TcpListener* server = new TcpListener(9999);
	if (!server->Listen())
	{
		return 0;
	};

	byte* raw_module = server->Accept();

	LOG("main ended");
	return 0;
}