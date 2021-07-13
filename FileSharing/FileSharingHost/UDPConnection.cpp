#include "UDPConnection.h"
#include "../Common/CommonFuction.h"

UDPConnection::UDPConnection(u_short srcPort, u_short destPort) : UDPConnection(srcPort, "192.168.1.255", destPort)
{
}

UDPConnection::UDPConnection(u_short srcPort, std::string destHost, u_short destPort)
{
	m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (m_sock == INVALID_SOCKET)
	{
		Throw(L"socket() error: " + std::to_wstring(WSAGetLastError()) + L".");
	}

	ULONG mode = 1;
	ioctlsocket(m_sock, FIONBIO, &mode);

	sockaddr_in* p = (sockaddr_in*)&m_address;

	p->sin_family = AF_INET;
	p->sin_port = htons(srcPort);
	p->sin_addr.s_addr = htonl(ADDR_ANY);

	p = (sockaddr_in*)&m_destAddr;

	p->sin_family = AF_INET;
	p->sin_port = htons(destPort);
	InetPtonA(AF_INET, destHost.c_str(), &p->sin_addr.s_addr);

	m_sockaddrLen = sizeof(sockaddr_in);

	auto re = bind(m_sock, (SOCKADDR*)&m_address, m_sockaddrLen);

	if (re == SOCKET_ERROR)
	{
		Throw(L"bind() error: " + std::to_wstring(WSAGetLastError()) + L".");
	}

}

UDPConnection::UDPConnection(std::string srcHost, u_short srcPort, std::string destHost, u_short destPort)
{
	m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (m_sock == INVALID_SOCKET)
	{
		Throw(L"socket() error: " + std::to_wstring(WSAGetLastError()) + L".");
	}

	ULONG mode = 1;
	ioctlsocket(m_sock, FIONBIO, &mode);

	sockaddr_in* p = (sockaddr_in*)&m_address;

	p->sin_family = AF_INET;
	p->sin_port = htons(srcPort);
	InetPtonA(AF_INET, srcHost.c_str(), &p->sin_addr.s_addr);

	p = (sockaddr_in*)&m_destAddr;

	p->sin_family = AF_INET;
	p->sin_port = htons(destPort);
	InetPtonA(AF_INET, destHost.c_str(), &p->sin_addr.s_addr);

	m_sockaddrLen = sizeof(sockaddr_in);

	auto re = bind(m_sock, (SOCKADDR*)&m_address, m_sockaddrLen);

	if (re == SOCKET_ERROR)
	{
		Throw(L"bind() error: " + std::to_wstring(WSAGetLastError()) + L".");
	}

}

UDPConnection::~UDPConnection()
{
	closesocket(m_sock);
}

void UDPConnection::SetMsg(const char* buffer, size_t size)
{
	m_sendBuffer = (char*)buffer;
	m_sendSize = size;
}

void UDPConnection::SetRecvBuffer(char* buffer, size_t size)
{
	m_recvBuffer = (char*)buffer;
	m_recvSize = size;
}

void UDPConnection::SetMode(int mode, ULONG enable)
{
	ioctlsocket(m_sock, mode, &enable);
}
