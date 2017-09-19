
#include "Debug.h"
#include "TcpListener.h"
#include "PeFile.h"
#include "PeDeserializer.h"

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

	PeFile* some_pe_file = new PeFile;
	PeDeserializer* deserializer = new PeDeserializer(raw_module, some_pe_file);
	some_pe_file = deserializer->Deserialize();
	
	LOG("main ended");
	return 0;
}