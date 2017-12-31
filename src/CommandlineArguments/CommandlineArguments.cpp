#include "CommandlineArguments.h"

CommandlineArguments::CommandlineArguments(int argc, char * argv[])
{
	argc_ = argc;
	argv_ = argv;
	desc = new boost::program_options::options_description("Options");
}

void CommandlineArguments::ProcessProgramOptions()
{
	desc->add_options()
		("help", "Print help messages")
		("port", boost::program_options::value<int>(), "Port that loader server will listen to")
		("local", "The dll will load to the loader process")
		("remote", "The dll will load to other process. pid is require")
		("pid", boost::program_options::value<int>(), "Process id to load the dll into");

	boost::program_options::store(boost::program_options::parse_command_line(argc_, argv_, *desc), vm_);
	
	if (vm_.count("help"))
	{
		PrintHelp();
		return;
	}
	if (vm_.count("local"))
	{
		CallLocalDllLoader();
		return;
	}
	if (vm_.count("remote"))
	{
		CallRemotellLoader();
		return;
	}
}

void CommandlineArguments::PrintHelp()
{
	std::cout << "Load a dll file received from the network, enabling to run the dll exports functions"
		<< std::endl << std::endl;
	desc->print(std::cout);
}

void CommandlineArguments::CallLocalDllLoader()
{
	if (vm_.count("port")) {
		std::cout << "Listenning to port " << vm_["port"].as<int>() << std::endl;
	}


	TcpListener* server = new TcpListener(vm_["port"].as<int>());
	if (!server->Listen())
	{
		return;
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
}

void CommandlineArguments::CallRemotellLoader()
{
	if (vm_.count("port")) {
		std::cout << "Listenning to port " << vm_["port"].as<int>() << std::endl;
	}
	if (vm_.count("pid")) {
		std::cout << "Inject dll to pid " << vm_["pid"].as<int>() << std::endl;
	}


	TcpListener* server = new TcpListener(vm_["port"].as<int>());
	if (!server->Listen())
	{
		return;
	};
	byte* raw_module = server->Accept();

	PeFile* some_pe_file = new PeFile;
	PeDeserializer* deserializer = new PeDeserializer(raw_module, some_pe_file);
	some_pe_file = deserializer->Deserialize();

	int process_id = vm_["pid"].as<int>();
	RemoteDllLoader* loader = new RemoteDllLoader(process_id, some_pe_file);
	loader->Load();
	loader->Attach();
	loader->CallLoudedFunctionByName("inc");

}

void CommandlineArguments::Start()
{
	LOG("CommandlineArguments::Start called");
	ProcessProgramOptions();
}
