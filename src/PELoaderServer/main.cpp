
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

	TcpListener* server = new TcpListener(9999);
	if (!server->tcpListen())
	{
		return 0;
	};
	byte* raw_module = server->tcpAccept();

	PeFile* some_pe_file = new PeFile;
	PeDeserializer* deserializer = new PeDeserializer(raw_module, some_pe_file);
	some_pe_file = deserializer->deserialize();

	Loader* loader = new Loader(some_pe_file);
	loader->load();
	loader->attach();
	
	PDWORD print_func = loader->getLoadedFunctionByName("print");
	PRINT printImportedFunc = (PRINT)print_func;
	printImportedFunc();
	ADD addImportedFunc = (ADD)loader->getLoadedFunctionByName("add");
	int ret = addImportedFunc(8, 2);
	LOG("addImportedFunc return: " + std::to_string(ret));
	MULT multImportedFunc = (MULT)loader->getLoadedFunctionByName("multiplication");
	ret = multImportedFunc(8, 2);
	LOG("multImportedFunc return: " + std::to_string(ret));

	LOG("main ended");
	return 0;
}
