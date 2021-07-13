#pragma once
#include <string>

#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <openssl/ossl_typ.h>


enum CONNECTION_OPT
{
	IPv4 = 1,
	IPv6 = 2,
	NON_BLOCKING_MODE = 4,
};

enum CONNECTION_IO_RETURN
{
#ifdef ERROR
#undef ERROR
#endif // ERROR

	ERROR = -1,
	WANT_WRITE = -2,
	WANT_READ = -3,
};

class FileSharingHost;

class Connection
{
private:
	friend std::string Request(std::string host, u_short port, long long opt, const std::string& msg);
	friend std::wstring Request(std::string host, u_short port, long long opt, const std::wstring& msg);

	friend class FileSharingHost;

private:
	SSL_CTX* m_ctx = nullptr;
	SSL* m_ssl = nullptr;

	SOCKET m_sock = 0;
	sockaddr_in6 m_address;

	int m_sockaddrLen = 0;

	bool m_isLoop = true;

	long long m_opt = 0;

public:
	std::string m_host;
	u_short m_port;

	std::string m_buffer;

public:
	//set opt for ip version
	Connection(std::string host, u_short port, long long opt = CONNECTION_OPT::IPv4);
	~Connection();

private:
	void InitAddress(std::string& host, u_short port, int af);
	void Connect();

public:
	long long Send(const char* buffer, size_t size);
	long long Recv(char* buffer, size_t size);

	std::string Request(const std::string& msg);

public:
	template<typename _Func, typename ..._Args>
	void Loop(_Func func, _Args&& ...args);
	inline void Stop() { m_isLoop = false; };

public:
	u_short LocalPort() { return m_port; };

	std::string& ServerHost() { return m_host; };
	u_short ServerPort();

};

template<typename _Func, typename ..._Args>
inline void Connection::Loop(_Func func, _Args&& ...args)
{
	static_assert(!(std::is_same_v<std::decay_t<_Func>, std::nullptr_t>));
	while (m_isLoop)
	{
		func(this, std::forward<_Args>(args) ...);
	}
}

#define REQUEST_BUFFER_SIZE 2048

std::string Request(std::string host, u_short port, long long opt, const std::string& msg);
std::wstring Request(std::string host, u_short port, long long opt, const std::wstring& msg);