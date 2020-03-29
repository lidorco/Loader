#pragma once

#include <string>

#define DEBUG

#ifdef DEBUG
	#define LOG(msg) logger.log(msg, __FILE__, __LINE__ )
#else
	#define LOG(msg) ;
#endif


class Debug {
public:
	static void log(const std::string&, const std::string&, const int);
	static void log(const std::string&);
	static void log(const int);
};

extern Debug logger;
