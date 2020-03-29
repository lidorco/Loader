
#include "TcpListener.h"
#include "Debug.h"

#ifndef __STRINGS_H_INCLUDED__
#define __STRINGS_H_INCLUDED__
#include <string>
#endif // __STRINGS_H_INCLUDED__

TcpListener::TcpListener(int port) : m_listening_port(port), m_listen_socket(INVALID_SOCKET)
{
	LOG("TcpListener created");
}

void TcpListener::initialize()
{
	WSAStartup(MAKEWORD(2, 2), &(m_wsa_data));

	std::string tmp_str_listen_port = std::to_string(m_listening_port);
	PCSTR str_listenning_port = tmp_str_listen_port.c_str();
	struct addrinfo hints;
	struct addrinfo *result = NULL;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(NULL, str_listenning_port, &hints, &result);

	m_listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	bind(m_listen_socket, result->ai_addr, (int)result->ai_addrlen);

	freeaddrinfo(result);
}


bool TcpListener::tcpListen() 
{
	initialize();

	LOG("TcpListener started listenning on port " + std::to_string(m_listening_port));
	int return_value = listen(m_listen_socket, SOMAXCONN);
	if (return_value)
	{
		LOG("Error occurred trying to listening in TcpListener");
		return false;
	}
	return true;
}


byte* TcpListener::tcpAccept() 
{
	SOCKET client_socket = INVALID_SOCKET;
	client_socket = accept(m_listen_socket, NULL, NULL);
	closesocket(m_listen_socket);
	LOG("TcpListener accepted client");
	
	// Receive until the peer shuts down the connection
	HANDLE process_heap = GetProcessHeap();
	HANDLE recvbuf = HeapAlloc(process_heap, HEAP_ZERO_MEMORY, DEFAULT_BUFFER_LENGTH);
	recv(client_socket, (char*)recvbuf, DEFAULT_BUFFER_LENGTH, 0);

	shutdown(client_socket, SD_SEND);
	closesocket(client_socket);
	WSACleanup();
	return (byte*)recvbuf;
}
