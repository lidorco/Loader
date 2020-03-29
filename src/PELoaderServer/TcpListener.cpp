
#include "TcpListener.h"
#include "Debug.h"

#include <string>

TcpListener::TcpListener(const int port) : m_listening_port(port), m_listen_socket(INVALID_SOCKET)
{
	LOG("TcpListener created");
}

void TcpListener::initialize()
{
	WSAStartup(MAKEWORD(2, 2), &(m_wsa_data));

	std::string tmpStrListenPort = std::to_string(m_listening_port);
	const auto strListenningPort = tmpStrListenPort.c_str();
	struct addrinfo hints;
	struct addrinfo *result = nullptr;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(nullptr, strListenningPort, &hints, &result);

	m_listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	bind(m_listen_socket, result->ai_addr, static_cast<int>(result->ai_addrlen));

	freeaddrinfo(result);
}


bool TcpListener::tcpListen() 
{
	initialize();

	LOG("TcpListener started listenning on port " + std::to_string(m_listening_port));
	const auto returnValue = listen(m_listen_socket, SOMAXCONN);
	if (returnValue)
	{
		LOG("Error occurred trying to listening in TcpListener");
		return false;
	}
	return true;
}


byte* TcpListener::tcpAccept() 
{
	SOCKET client_socket = INVALID_SOCKET;
	client_socket = accept(m_listen_socket, nullptr, nullptr);
	closesocket(m_listen_socket);
	LOG("TcpListener accepted client");
	
	// Receive until the peer shuts down the connection
	const auto processHeap = GetProcessHeap();
	const auto receivedBuffer = HeapAlloc(processHeap, HEAP_ZERO_MEMORY, DEFAULT_BUFFER_LENGTH);
	recv(client_socket, static_cast<char*>(receivedBuffer), DEFAULT_BUFFER_LENGTH, 0);

	shutdown(client_socket, SD_SEND);
	closesocket(client_socket);
	WSACleanup();
	return static_cast<byte*>(receivedBuffer);
}
