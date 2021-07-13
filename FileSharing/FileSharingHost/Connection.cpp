#include "Connection.h"

#include <cassert>
#include <iostream>

#include "CommonSSL.h"

#include "../Common/CommonFuction.h"


#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")


Connection::Connection(std::string host, u_short port, long long opt)
{
	m_host = host;
	m_opt = opt;

	int af = -1;

	//init socket
	if ((opt & CONNECTION_OPT::IPv4) != 0)
	{
		af = AF_INET;
	}
	else if((opt& CONNECTION_OPT::IPv6) != 0)
	{
		af = AF_INET6;
	}

	m_sock = socket(af, SOCK_STREAM, IPPROTO_TCP);

	//set opt
	if ((opt & CONNECTION_OPT::NON_BLOCKING_MODE) != 0)
	{
		ULONG mode = 1;
		ioctlsocket(m_sock, FIONBIO, &mode);
	}

	InitAddress(host, port, af);

	const SSL_METHOD* meth = TLSv1_2_client_method();
	m_ctx = SSL_CTX_new(meth);
	m_ssl = SSL_new(m_ctx);

	//m_sslFd = SSL_get_fd(m_ssl);
	SSL_set_fd(m_ssl, m_sock);

	if (!m_ssl)
	{
		LogSSL();
		Throw(L"SSL_new() error.");
	}

	m_buffer.resize(REQUEST_BUFFER_SIZE);

	Connect();

}

Connection::~Connection()
{
	if (m_ssl) SSL_free(m_ssl);
	if (m_ctx) SSL_CTX_free(m_ctx);
	closesocket(m_sock);
}

void Connection::InitAddress(std::string& host, u_short port, int af)
{
	if (af == AF_INET)
	{
		sockaddr_in* p = (sockaddr_in*)&m_address;

		p->sin_family = af;
		p->sin_port = htons(port);

		InetPtonA(af, host.c_str(), &p->sin_addr.s_addr);
		m_sockaddrLen = sizeof(sockaddr_in);
	}
	else
	{
		sockaddr_in6* p = (sockaddr_in6*)&m_address;

		p->sin6_family = af;
		p->sin6_port = htons(port);

		InetPtonA(af, host.c_str(), &p->sin6_addr.s6_addr);
		m_sockaddrLen = sizeof(sockaddr_in6);
	}
	

}

void Connection::Connect()
{
	sockaddr_in6 returnServ;

	if (connect(m_sock, (SOCKADDR*)&m_address, m_sockaddrLen) == SOCKET_ERROR)
	{
		Throw(L"connect() error: " + std::to_wstring(WSAGetLastError()) + L".");
	}

	if (getsockname(m_sock, (sockaddr*)&returnServ, &m_sockaddrLen) == SOCKET_ERROR)
	{
		Throw(L"getsockname() error: " + std::to_wstring(WSAGetLastError()) + L".");
	}
	else
	{
		m_port = ntohs(returnServ.sin6_port);
	}

	int err = SSL_connect(m_ssl);

	if (err <= 0) 
	{
		LogSSL();
		Throw(L"SSL_connect() error.");
	}

}

long long Connection::Send(const char* buffer, size_t size)
{
	int len = SSL_write(m_ssl, buffer, size);

	if (len < 0)
	{
		int err = SSL_get_error(m_ssl, len);
		switch (err)
		{
		case SSL_ERROR_WANT_WRITE:
			//std::cout << "SSL_write(): SSL_ERROR_WANT_WRITE\n";
			return -2;
		case SSL_ERROR_WANT_READ:
			//std::cout << "SSL_write(): SSL_ERROR_WANT_READ\n";
			return -3;
		case SSL_ERROR_ZERO_RETURN:
			//std::cout << "SSL_write(): SSL_ERROR_ZERO_RETURN\n";
		case SSL_ERROR_SYSCALL:
			//std::cout << "SSL_write(): SSL_ERROR_SYSCALL\n";
		case SSL_ERROR_SSL:
			//std::cout << "SSL_write(): SSL_ERROR_SSL\n";
		default:
			return -1;
		}
		return len;
	}

	return len;
}

long long Connection::Recv(char* buffer, size_t size)
{
	int len = SSL_read(m_ssl, buffer, size);

	if (len < 0)
	{
		int err = SSL_get_error(m_ssl, len);
		switch (err)
		{
		case SSL_ERROR_WANT_WRITE:
			//std::cout << "SSL_read(): SSL_ERROR_WANT_WRITE\n";
			return -2;
		case SSL_ERROR_WANT_READ:
			//std::cout << "SSL_read(): SSL_ERROR_WANT_READ\n";
			return -3;
		case SSL_ERROR_ZERO_RETURN:
			//std::cout << "SSL_read(): SSL_ERROR_ZERO_RETURN\n";
		case SSL_ERROR_SYSCALL:
			//std::cout << "SSL_read(): SSL_ERROR_SYSCALL\n";
		case SSL_ERROR_SSL:
			//std::cout << "SSL_read(): SSL_ERROR_SSL\n";
		default:
			return -1;
		}
		return len;
	}

	buffer[len] = '\0';

	return len;
}

std::string Connection::Request(const std::string& msg)
{
	bool nbm = (m_opt & CONNECTION_OPT::NON_BLOCKING_MODE) != 0;
	ULONG mode = 0;

	if (nbm)
	{
		ioctlsocket(m_sock, FIONBIO, &mode);
	}

	if (Send(msg.c_str(), msg.size()) == -1)
	{
		return "Error";
	}

	std::string respone;

	mode = 1;
	ioctlsocket(m_sock, FIONBIO, &mode);

	while (!IsReadyRead(m_sock, 0, 16)) { Sleep(32); };
	auto re = Recv(&m_buffer[0], REQUEST_BUFFER_SIZE);

	auto limitWait = 20;

	while (true)
	{
		if (re > 0) respone.append(&m_buffer[0], re);
		else if (re == 0) break;
		else if (re < 0)
		{
			limitWait--;
			if (limitWait == 0) break;
			if (re == CONNECTION_IO_RETURN::ERROR)
			{
				break;
			}
			Sleep(16);
		}
		re = Recv(&m_buffer[0], REQUEST_BUFFER_SIZE);
	}

	if (!nbm)
	{
		mode = 0;
		ioctlsocket(m_sock, FIONBIO, &mode);
	}

	return respone;
}

u_short Connection::ServerPort()
{
	return m_sockaddrLen == sizeof(sockaddr_in) ? 
		ntohs(((sockaddr_in*)&m_address)->sin_port) : ntohs(((sockaddr_in6*)&m_address)->sin6_port);
}

std::string Request(std::string host, u_short port, long long opt, const std::string& msg)
{
	Connection* conn = new Connection(host, port, opt);

	if (conn->Send(msg.c_str(), msg.size()) == -1)
	{
		delete conn;
		return "Send() request error.";
	}

	std::string respone;

	ULONG mode = 1;
	ioctlsocket(conn->m_sock, FIONBIO, &mode);

	char* buffer = new char[REQUEST_BUFFER_SIZE];

	while (!IsReadyRead(conn->m_sock, 0, 16)) { Sleep(32); };
	auto re = conn->Recv(buffer, REQUEST_BUFFER_SIZE);

	auto limitWait = 20;

	while (true)
	{
		if (re > 0) respone.append(buffer, re);
		else if (re == 0) break;
		else if (re < 0)
		{
			limitWait--;
			if(limitWait == 0) break;
			if (re == CONNECTION_IO_RETURN::ERROR)
			{
				break;
			}
			Sleep(16);
		}
		re = conn->Recv(buffer, REQUEST_BUFFER_SIZE);
	}

	delete[] buffer;
	delete conn;
	
	return respone;
}


std::wstring Request(std::string host, u_short port, long long opt, const std::wstring& msg)
{
	Connection* conn = new Connection(host, port, opt);

	if (conn->Send((char*)&msg[0], msg.size() * 2 + 2) == -1)
	{
		delete conn;
		return L"Send() request error.";
	}

	std::wstring respone;

	ULONG mode = 1;
	ioctlsocket(conn->m_sock, FIONBIO, &mode);

	char* buffer = new char[REQUEST_BUFFER_SIZE + 25]();

	while (!IsReadyRead(conn->m_sock, 0, 16)) { Sleep(32); };

	auto re = conn->Recv(buffer, REQUEST_BUFFER_SIZE);

	auto limitWait = 20;

	auto offset = 0;

	while (true)
	{
		if (re > 0)
		{
			offset = re % 2 == 0 ? 0 : 1;

			respone.append((wchar_t*)&buffer[0], (re - offset) / 2);
		}
		else if (re == 0) break;
		else if (re < 0)
		{
			limitWait--;
			if (limitWait == 0) break;
			if (re == CONNECTION_IO_RETURN::ERROR)
			{
				break;
			}
			Sleep(16);
		}
		re = conn->Recv(buffer + offset, REQUEST_BUFFER_SIZE - offset);
	}

	delete[] buffer;
	delete conn;

	return respone;
}
