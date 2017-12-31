
#include "CommandlineArguments.h"

Debug logger;

int main(int argc, char *argv[])
{
	LOG("main started");

	CommandlineArguments *program = new CommandlineArguments(argc, argv);
	program->Start();

	LOG("main ended");
	return 0;
}
