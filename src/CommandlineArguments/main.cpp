
#include "../PELoaderServer/Debug.h"
#include "../PELoaderServer/TcpListener.h"
#include "../PELoaderServer/PeFile.h"
#include "../PELoaderServer/PeDeserializer.h"
#include "../PELoaderServer/LocalDllLoader.h"
#include "../PELoaderServer/RemoteDllLoader.h"


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

	int process_id = 656;
	RemoteDllLoader* loader = new RemoteDllLoader(process_id, some_pe_file);
	loader->Load();
	loader->Attach();
	loader->CallLoudedFunctionByName("inc");

	LOG("main ended");
	return 0;
}
