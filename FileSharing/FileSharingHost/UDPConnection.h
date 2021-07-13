#pragma once
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "../Common/CommonFuction.h"

#define _NON_BLOCKING_MODE FIONBIO

//IPv4 only
class UDPConnection
{
public:
	SOCKET m_sock;

	sockaddr_in6 m_address = {};

	sockaddr_in6 m_destAddr = {};

	int m_sockaddrLen;

	bool m_isLoop = true;

public:
	char* m_sendBuffer = nullptr;
	int m_sendSize = 0;

	char* m_recvBuffer = nullptr;
	int m_recvSize = 0;

public:
	UDPConnection(u_short srcPort, u_short destPort);
	UDPConnection(u_short srcPort, std::string destHost, u_short destPort);

	UDPConnection(std::string srcHost, u_short srcPort, std::string destHost, u_short destPort);

	~UDPConnection();

public:
	void SetMsg(const char* buffer, size_t size);
	void SetRecvBuffer(char* buffer, size_t size);

	//NON_BLOCKING_MODE, ...
	void SetMode(int mode, ULONG enable);

public:
	template<typename _Func1, typename _Func2, typename ..._Args>
	void Loop(_Func1 send, _Func2 recv, _Args&& ...args);

	template<typename _Func, typename ..._Args>
	void Recv(_Func recv, _Args&& ...args);

	template<typename _Func, typename ..._Args>
	void Send(_Func recv, _Args&& ...args);

	inline void Stop() { m_isLoop = false; };

	inline sockaddr_in6 GetDestinationHost() { return m_destAddr; };
	inline void SetDestinationHost(sockaddr_in6* dest) { m_destAddr = *dest; };

};

template<typename _Func1, typename _Func2, typename ..._Args>
inline void UDPConnection::Loop(_Func1 send, _Func2 recv, _Args&& ...args)
{
	m_isLoop = true;
	while (m_isLoop)
	{
		if (IsReadyWrite(m_sock, 0, 16))
		{
			if(m_sendBuffer != nullptr)
			if (sendto(m_sock, m_sendBuffer, m_sendSize, 0, (SOCKADDR*)&m_destAddr, m_sockaddrLen) != SOCKET_ERROR)
			{
				if constexpr (!(std::is_same_v<std::decay_t<_Func1>, std::nullptr_t>))
					send(std::forward<_Args>(args) ...);
			}
		}
		
		if constexpr (!(std::is_same_v<std::decay_t<_Func2>, std::nullptr_t>))
		if (IsReadyRead(m_sock, 0, 16))
		{
			if (m_recvBuffer != nullptr)
			{
				auto re = recvfrom(m_sock, m_recvBuffer, m_recvSize, 0, (SOCKADDR*)&m_address, &m_sockaddrLen);
				if (re == SOCKET_ERROR)
				{
					continue;
				}
				while (re > 0)
				{
					m_recvBuffer[re] = '\0';
					recv(&m_address, re, std::forward<_Args>(args) ...);
					re = recvfrom(m_sock, m_recvBuffer, m_recvSize, 0, (SOCKADDR*)&m_address, &m_sockaddrLen);
					if (re == SOCKET_ERROR)
					{
						break;
					}
				}
				/*if (re != SOCKET_ERROR)
				{
					m_recvBuffer[re] = '\0';
					recv(&m_address, re, std::forward<_Args>(args) ...);
				}*/
			}
			
		}
	}
}

template<typename _Func, typename ..._Args>
inline void UDPConnection::Recv(_Func recv, _Args&& ...args)
{
	if constexpr (!(std::is_same_v<std::decay_t<_Func>, std::nullptr_t>))
		if (IsReadyRead(m_sock, 0, 16))
		{
			if (m_recvBuffer != nullptr)
			{
				auto re = recvfrom(m_sock, m_recvBuffer, m_recvSize, 0, (SOCKADDR*)&m_address, &m_sockaddrLen);
				if (re != SOCKET_ERROR)
				{
					m_recvBuffer[re] = '\0';
					recv(&m_address, re, std::forward<_Args>(args) ...);
				}
			}

		}
}

template<typename _Func, typename ..._Args>
inline void UDPConnection::Send(_Func recv, _Args&& ...args)
{
	if (IsReadyWrite(m_sock, 0, 16))
	{
		if (m_sendBuffer != nullptr)
			if (sendto(m_sock, m_sendBuffer, m_sendSize, 0, (SOCKADDR*)&m_destAddr, m_sockaddrLen) != SOCKET_ERROR)
			{
				if constexpr (!(std::is_same_v<std::decay_t<_Func>, std::nullptr_t>))
					send(std::forward<_Args>(args) ...);
			}
	}
}
