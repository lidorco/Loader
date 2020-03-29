#include "Debug.h"

#include <iostream>


void Debug::log(const std::string& logMsg)
{
	std::cout << logMsg;
}

void Debug::log(const int logMsg)
{
	std::cout << logMsg;
}

void Debug::log(const std::string& logMsg, const std::string& fileName, const int lineNumber)
{
	std::cout << fileName << "(" << lineNumber << ") " << logMsg << std::endl;
}
