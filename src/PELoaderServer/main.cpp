
#include "Debug.h"
#include "TcpListener.h"
#include "PeFile.h"
#include "PeDeserializer.h"
#include "LocalDllLoader.h"

Debug logger;

typedef void(*PRINT)();
typedef int(*ADD)(int, int);
typedef int(*MULT)(int, int);

int main()
{
	LOG("main started");

	TcpListener* server = new TcpListener(9999);
	if (!server->Listen())
	{
		return 0;
	};
	byte* raw_module = server->Accept();

	PeFile* some_pe_file = new PeFile;
	PeDeserializer* deserializer = new PeDeserializer(raw_module, some_pe_file);
	some_pe_file = deserializer->Deserialize();

	LocalDllLoader* loader = new LocalDllLoader(some_pe_file);
	loader->Load();
	loader->Attach();
	
	PDWORD print_func = loader->GetLoudedFunctionByName("print");
	PRINT printImportedFunc = (PRINT)print_func;
	printImportedFunc();
	ADD addImportedFunc = (ADD)loader->GetLoudedFunctionByName("add");
	int ret = addImportedFunc(8, 2);
	LOG("addImportedFunc return: " + std::to_string(ret));
	MULT multImportedFunc = (MULT)loader->GetLoudedFunctionByName("multiplication");
	ret = multImportedFunc(8, 2);
	LOG("multImportedFunc return: " + std::to_string(ret));

	LOG("main ended");
	return 0;
}
