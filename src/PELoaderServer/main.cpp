
#include "Debug.h"
#include "TcpListener.h"
#include "PeFile.h"
#include "PeDeserializer.h"
#include "Loader.h"

Debug logger;

typedef void(*PRINT)();
typedef int(*ADD)(int, int);
typedef int(*MULT)(int, int);

int main()
{
	LOG("main started");

	auto server = new TcpListener(9999);
	if (!server->tcpListen())
	{
		return 0;
	}
	const auto rawModule = server->tcpAccept();

	auto somePeFile = new PeFile;
	auto deserializer = new PeDeserializer(rawModule, somePeFile);
	somePeFile = deserializer->deserialize();

	auto loader = new Loader(somePeFile);
	loader->load();

	loader->attach();
	
	loader->run("testFunction");

	LOG("main ended");
	return 0;
}
