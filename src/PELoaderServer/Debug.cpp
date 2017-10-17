#include "Debug.h"

#ifndef __IOSTREAM_H_INCLUDED__
#define __IOSTREAM_H_INCLUDED__
#include <iostream>
#endif // __IOSTREAM_H_INCLUDED__


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
