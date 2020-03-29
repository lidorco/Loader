#pragma once


#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFFER_LENGTH 65536


class TcpListener {	

public:
	TcpListener(int);
	bool tcpListen();
	byte* tcpAccept();
private:
	void initialize();
	int m_listening_port;
	SOCKET m_listen_socket;
	WSADATA m_wsa_data;
};
