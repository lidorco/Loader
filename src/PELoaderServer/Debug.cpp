#include "Debug.h"

#include <iostream>


void Debug::log(const std::string& log_msg)
{
	std::cout << log_msg;
}

void Debug::log(const int log_msg)
{
	std::cout << log_msg;
}

void Debug::log(const std::string& log_msg, const std::string& file_name, const int line_number)
{
	std::cout << file_name << "(" << line_number << ") " << log_msg << std::endl;
}
