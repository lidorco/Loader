#pragma once

#ifndef __WINSOCKS_H_INCLUDED__
#define __WINSOCKS_H_INCLUDED__
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")
#endif // __WINSOCKS_H_INCLUDED__ 

#define DEFAULT_BUFFER_LENGTH 65536


class TcpListener {	

public:
	TcpListener(int);
	bool Listen();
	byte* Accept();
private:
	void Initialize();
	int listening_port_;
	SOCKET listen_socket_;
	WSADATA wsa_data_;
};
