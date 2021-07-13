#pragma once
#include <string>
#include <vector>

#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <openssl/ossl_typ.h>
#include <thread>
#include <openssl/ssl.h>
#include <map>
#include <queue>
#include <mutex>
#include <functional>

#include <iostream>

#include "ConsoleIO.h"

//#include "../Common/CommonFuction.h"

#define MAX_CLIENT 32

class Connection;

void Release(std::mutex& locker);

enum SERVER_OPT
{
	SERVER_IPv4 = 1,
	SERVER_IPv6 = 2,
	SERVER_NON_BLOCKING_MODE = 4,
};

struct ClientResource
{
	std::string request;
	std::string response;

	//inheritance must override it to free resource
	inline virtual ~ClientResource() 
	{ 
		auto t1 = std::string("");
		auto t2 = std::string("");
		request.swap(t1);
		response.swap(t2);
	};
};

class ServerThread;

struct ClientConnection
{
	SOCKET sock;
	sockaddr_in6 address = {};
	SSL* ssl;
	ServerThread* serv;

	//std::function<void()> servFunc;

	char buffer[1024] = {};
	int bufferSize = 1024;

	ClientResource* resource = nullptr;

	inline ClientConnection() { bufferSize = 1024; };

	template<typename T>
	inline T* Address()
	{
		return (T*)&address;
	}

	inline long long Send(const char* buffer, size_t size)
	{
		int len = SSL_write(ssl, buffer, size);

		if (len < 0)
		{
			int err = SSL_get_error(ssl, len);
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

	inline long long Recv(char* buffer, size_t size)
	{
		int len = SSL_read(ssl, buffer, size);

		if (len < 0)
		{
			int err = SSL_get_error(ssl, len);
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

		return len;
	}

};

class Server;

void Loop(ServerThread* serv);
void _Loop(ServerThread* serv);

class ServerThread
{
private:
	friend void _Loop(ServerThread* serv);
	friend void Loop(ServerThread* serv);
	friend class Server;

	//std::mutex m_lock;
	//port, client
	std::map<u_short, ClientConnection*> m_conns;

	std::queue<ClientConnection*> m_trash;

public:
	Server* m_parent;

	//unique ip address
	IN6_ADDR m_address;
	bool m_isLoop = true;

	std::thread* m_servThr;

	ClientConnection* m_iter;

	//std::function<void()> m_servFunc;
	void(*m_servFunc)(Server*, ServerThread*, ClientConnection*, ClientResource*) = nullptr;

	std::mutex m_incomingLock;
	std::queue<ClientConnection*> m_incoming;

	void(*m_resourceDeleter)(ClientResource**) = nullptr;

private:
	void GarbageCollect();

public:
	bool Add(ClientConnection* conn);

	~ServerThread();

};

class Server
{
private:
	friend class ServerThread;

public:
	SSL_CTX* m_ctx;

	SOCKET m_sock = 0;
	sockaddr_in6 m_address;

	int m_addrLen = 0;

	//std::vector<ClientConnection> m_clients;

	std::map<IN6_ADDR, ServerThread*, bool(*)(const IN6_ADDR&, const IN6_ADDR&)> m_clientMap;

	bool m_isLoop = true;

	std::thread* m_garbageCollector = nullptr;

	std::mutex m_lock;
	//critical resource
	std::queue<IN6_ADDR> m_trash;

	long long m_opt = 0;

public:
	std::string m_host;
	u_short m_port;

public:
	Server(std::string host, u_short port, long long opt = SERVER_OPT::SERVER_IPv4);
	~Server();

private:
	ServerThread* Listen();

	void StartGarbageCollector();

	void Stop(ServerThread* serv);

	ServerThread* Listen(ClientResource*, void(*resourceDeleter)(ClientResource**));

	template<typename T>
	ServerThread* _Listen();

public:
	void ForceStop(ServerThread* serv);

public:
	template<typename _Func, typename ..._Args>
	void Listen(_Func func, _Args&& ...args);

	//listen, accept and allocate ClientResource (or child class of ClientResource) for new connect 
	template<typename T>
	void Listen(void(*)(Server*, ServerThread*, ClientConnection*, ClientResource*));

public:
	void Close(ClientConnection* conn);
	void Stop() { m_isLoop = false; };

};

template<typename T>
inline ServerThread* Server::_Listen()
{
	ClientConnection* c = new ClientConnection();

	ClientConnection& client = *c;

	memset(&client.address, 0, m_addrLen);
	client.sock = accept(m_sock, (SOCKADDR*)&client.address, &m_addrLen);

	ULONG mode = 1;
	ioctlsocket(client.sock, FIONBIO, &mode);

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(client.sock, &fds);

	fd_set fdsr;
	FD_ZERO(&fdsr);
	FD_SET(client.sock, &fdsr);

	select(0, &fds, &fdsr, 0, 0);

	bool hasReturn = false;

	IN6_ADDR* p;
	if (client.sock == INVALID_SOCKET)
	{
		Throw(L"accept() error.");
	}

	client.ssl = SSL_new(m_ctx);

	//client.sslFd = SSL_get_fd(client.ssl);
	SSL_set_fd(client.ssl, client.sock);

	if (!client.ssl)
	{
		Throw(L"SSL_new() error.");
	}

	auto acc = SSL_accept(client.ssl);

	if (acc == 0)
	{
		/* Hard error */
		closesocket(client.sock);
		SSL_free(client.ssl);
		SafeDelete(c);
		return 0;
	}
	else if (acc == -1)
	{
		int err = SSL_get_error(client.ssl, acc);
		if (err == SSL_ERROR_WANT_READ)
		{
			/* Wait for data to be read */
		}
		else if (err == SSL_ERROR_WANT_WRITE)
		{
			/* Write data to continue */
		}
		else if (err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL)
		{
			closesocket(client.sock);
			SSL_clear(client.ssl);
			SafeDelete(c);
			return 0;
		}
		else if (err == SSL_ERROR_ZERO_RETURN)
		{
			closesocket(client.sock);
			SSL_clear(client.ssl);
			SafeDelete(c);
			return 0;
		}
	}

	char buffer[256] = {};
	int af = 0;

	u_short port = 0;
	if ((m_opt & SERVER_OPT::SERVER_IPv6) != 0)
	{
		p = &client.address.sin6_addr;
		af = AF_INET6;
		port = client.address.sin6_port;
	}
	else
	{
		p = (IN6_ADDR*)&((sockaddr_in*)&client.address)->sin_addr;
		af = AF_INET;
		port = client.address.sin6_port;
	}

	InetNtopA(af, p, buffer, 256);
	ConsoleLog("Connection request\t\t: [" << buffer << ":" << ntohs(port) << "]\n");

	client.resource = new T();

	if (WaitToLock(m_lock))
	{
		auto it = m_clientMap.find(*p);

		if (it != m_clientMap.end())
		{
			it->second->Add(c);
			hasReturn = true;
		}
		Release(m_lock);
	}

	ConsoleLog("Accept connection\t\t: [" << buffer << ":" << ntohs(port) << "]\n");

	if (!hasReturn)
	{
		if (WaitToLock(m_lock))
		{
			ServerThread* serv = new ServerThread();
			serv->m_address = *p;
			serv->Add(c);

			serv->m_parent = this;

			m_clientMap.insert({ *p,serv });

			return serv;
		}
	}

	return 0;
}

//procotype auto FuncName(Server* sevr, ServerThread* thread, ClientConncetion* conn, ...);
template<typename _Func, typename ..._Args>
inline void Server::Listen(_Func func, _Args&& ...args)
{
	static_assert(!(std::is_same_v<std::decay_t<_Func>, std::nullptr_t>));
	StartGarbageCollector();
	while (m_isLoop)
	{
		auto serv = Listen();
		if (serv)
		{
			//serv->m_servFunc = std::bind(func, this, serv, &serv->m_iter, std::forward<_Args>(args) ...);
			serv->m_incoming.front()->servFunc = serv->m_servFunc;
			serv->m_servThr = new std::thread(Loop, serv);
			serv->m_servThr->detach();
			Release(m_lock);
		}
	}

	/*for (auto& c : m_clientMap)
	{
		auto re = TerminateThread(c.second.servThr->native_handle(), 1);
		delete c.second.servThr;
	}*/

}

template<typename T>
inline void Server::Listen(void(*func)(Server*, ServerThread*, ClientConnection*, ClientResource*))
{
	static_assert(std::is_base_of<ClientResource, T>::value == true);

	StartGarbageCollector();

	void(*deleter)(ClientResource**)= [](ClientResource** r)
	{
		delete dynamic_cast<T*>(*r);
		*r = nullptr;
	};

	while (m_isLoop)
	{
		//auto serv = Listen((ClientResource*)new T(), deleter);
		auto serv = _Listen<T>();

		if (serv)
		{
			serv->m_resourceDeleter = deleter;
			serv->m_servFunc = func;
			serv->m_servThr = new std::thread(_Loop, serv);
			serv->m_servThr->detach();
			Release(m_lock);
		}
	}
}