
#include "TcpListener.h"
#include "Debug.h"

#ifndef __STRINGS_H_INCLUDED__
#define __STRINGS_H_INCLUDED__
#include <string>
#endif // __STRINGS_H_INCLUDED__

TcpListener::TcpListener(int port) : listening_port_(port), listen_socket_(INVALID_SOCKET)
{
	LOG("TcpListener created");
}

void TcpListener::Initialize()
{
	WSAStartup(MAKEWORD(2, 2), &(wsa_data_));

	std::string tmp_str_listen_port = std::to_string(listening_port_);
	PCSTR str_listenning_port = tmp_str_listen_port.c_str();
	struct addrinfo hints;
	struct addrinfo *result = NULL;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(NULL, str_listenning_port, &hints, &result);

	listen_socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	bind(listen_socket_, result->ai_addr, (int)result->ai_addrlen);

	freeaddrinfo(result);
}


bool TcpListener::Listen() 
{
	Initialize();

	LOG("TcpListener started listenning on port " + std::to_string(listening_port_));
	int return_value = listen(listen_socket_, SOMAXCONN);
	if (return_value)
	{
		LOG("Error occurred trying to listening in TcpListener");
		return false;
	}
	return true;
}


byte* TcpListener::Accept() 
{
	SOCKET client_socket = INVALID_SOCKET;
	client_socket = accept(listen_socket_, NULL, NULL);
	closesocket(listen_socket_);
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
