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
	void log(const std::string&, const std::string&, const int);
	void log(const std::string&);
	void log(const int);
};

extern Debug logger;
