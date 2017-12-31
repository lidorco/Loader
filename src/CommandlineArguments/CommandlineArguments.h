#pragma once

#include <iostream>

#include <boost/program_options.hpp>

#include "../PELoaderServer/Debug.h"
#include "../PELoaderServer/TcpListener.h"
#include "../PELoaderServer/PeFile.h"
#include "../PELoaderServer/PeDeserializer.h"
#include "../PELoaderServer/LocalDllLoader.h"
#include "../PELoaderServer/RemoteDllLoader.h"


typedef void(*PRINT)();
typedef int(*ADD)(int, int);
typedef int(*MULT)(int, int);


class CommandlineArguments {

public:
	CommandlineArguments(int argc, char *argv[]);
	void Start();

private:
	int argc_;
	char **argv_;
	boost::program_options::options_description *desc;
	boost::program_options::variables_map vm_;

	void ProcessProgramOptions();
	void PrintHelp();
	void CallLocalDllLoader();
	void CallRemotellLoader();
};