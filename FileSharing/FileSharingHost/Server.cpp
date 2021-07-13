#include "Server.h"

#include "CommonSSL.h"


Server::Server(std::string host, u_short port, long long opt)
{
	m_port = port;

	m_clientMap = std::map<IN6_ADDR, ServerThread*, bool(*)(const IN6_ADDR&, const IN6_ADDR&)>(
		[](const IN6_ADDR& a, const IN6_ADDR& b)->bool
		{
			return memcmp(&a, &b, sizeof(IN6_ADDR)) > 0;
		}
		);

	m_host = host;

	int af = -1;

	m_opt = opt;

	//init socket
	if ((opt & SERVER_OPT::SERVER_IPv4) != 0)
	{
		af = AF_INET;
		sockaddr_in* p = (sockaddr_in*)&m_address;
		p->sin_family = af;
		p->sin_port = htons(port);
		p->sin_addr.s_addr = htonl(INADDR_ANY);
		m_addrLen = sizeof(sockaddr_in);
	}
	else if ((opt & SERVER_OPT::SERVER_IPv6) != 0)
	{
		af = AF_INET6;
		sockaddr_in6* p = (sockaddr_in6*)&m_address;
		p->sin6_family = af;
		p->sin6_port = htons(port);
		p->sin6_addr = in6addr_any;
		m_addrLen = sizeof(sockaddr_in6);
	}

	m_sock = socket(af, SOCK_STREAM, IPPROTO_TCP);

	char yes = 1;
	setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	//set opt
	if ((opt & SERVER_OPT::SERVER_NON_BLOCKING_MODE) != 0)
	{
		ULONG mode = 1;
		ioctlsocket(m_sock, FIONBIO, &mode);
	}

	const SSL_METHOD* meth = TLS_method();
	m_ctx = SSL_CTX_new(meth);


	if (bind(m_sock, (SOCKADDR*)&m_address, m_addrLen) == SOCKET_ERROR)
	{
		Throw(L"bind() error: " + std::to_wstring(WSAGetLastError()) + L".");
	}
#ifdef _DEBUG
	if (SSL_CTX_use_certificate_file(m_ctx, "D:\\20202\\Cryptography\\FileSharing\\Resource\\X509\\test.der", SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		exit(3);
	}

	if (SSL_CTX_use_PrivateKey_file(m_ctx, "D:\\20202\\Cryptography\\FileSharing\\Resource\\X509\\test.key", SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		exit(4);
	}
#else
	if (SSL_CTX_use_certificate_file(m_ctx, "test.der", SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		exit(3);
	}

	if (SSL_CTX_use_PrivateKey_file(m_ctx, "test.key", SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		exit(4);
	}
#endif // _DEBUG

	

	if (listen(m_sock, MAX_CLIENT) == SOCKET_ERROR)
	{
		Throw(L"listen() error: " + std::to_wstring(WSAGetLastError()) + L".");
	}

}

Server::~Server()
{
}


ServerThread* Server::Listen()
{
	ClientConnection* c = new ClientConnection();
	ClientConnection& client = *c;

	/*if (seRet != 1)
	{
		Throw(L"select() error: " + std::to_wstring(WSAGetLastError()) + L".");
	}*/

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
		LogSSL();
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
	std::cout << "Client request: " << buffer << ":" << ntohs(port) << "\n";

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

	std::cout << "Accept client: " << buffer << ":" << ntohs(port) << "\n";

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

void Server::StartGarbageCollector()
{
	m_garbageCollector = new std::thread(
		[&]() 
		{
			while (m_isLoop)
			{
				if (m_trash.empty())
				{
					Sleep(16);
					continue;
				}
				if (WaitToLock(m_lock, 16, LLONG_MAX))
				{
					while (!m_trash.empty())
					{
						auto& top = m_trash.front();

						auto it = m_clientMap.find(top);

						if (it != m_clientMap.end())
						{
							if (it->second->m_servThr->joinable()) it->second->m_servThr->join();

							if (it->second->m_incoming.empty())
							{
								SafeDelete(it->second);
								m_clientMap.erase(it);
							}
							else
							{
								it->second->m_isLoop = true;

								SafeDelete(it->second->m_servThr);

								it->second->m_servThr = new std::thread(_Loop, it->second);
								it->second->m_servThr->detach();
							}
							
						}

						m_trash.pop();
					}
					Release(m_lock);
				}
			}
		});

	m_garbageCollector->detach();
}

void Server::Stop(ServerThread* serv)
{
	if (WaitToLock(m_lock, 16, LLONG_MAX))
	{
		m_trash.push(serv->m_address);
		serv->m_isLoop = false;
		Release(m_lock);
	}
}

void Server::ForceStop(ServerThread* serv)
{
	if (WaitToLock(m_lock))
	{
		auto it = m_clientMap.find(serv->m_address);

		if (it != m_clientMap.end())
		{
			SafeDelete(serv);
			m_clientMap.erase(it);
		}

		Release(m_lock);
	}
}

void Loop(ServerThread* serv)
{
	try
	{
		while (serv->m_isLoop)
		{
			for (auto it = serv->m_conns.begin();
				it != serv->m_conns.end(); it++)
			{
				serv->m_iter = it->second;
				if (serv->m_iter->ssl != 0 && serv->m_iter->bufferSize != 0)
				{
					//serv->m_iter->servFunc();
				}
				else
				{
					serv->m_parent->Close(serv->m_iter);
				}
			}
			serv->m_iter = nullptr;
			serv->GarbageCollect();
		}
	}
	catch (...)
	{
		serv->m_parent->ForceStop(serv);
	}
	
}

void Server::Close(ClientConnection* conn)
{
	if (conn->bufferSize == 0) return;
	conn->bufferSize = 0;
	conn->serv->m_trash.push(conn);
}

void ServerThread::GarbageCollect()
{
	char buffer[256] = {};

	while (!m_trash.empty())
	{
		auto top = m_trash.front();

		auto it = m_conns.find(top->address.sin6_port);

		if (it != m_conns.end())
		{
			/*TerminateThread(it->second.servThr->native_handle(), 1);*/

			if (top->ssl)
			{
				SSL_shutdown(top->ssl);
				SSL_clear(top->ssl);
			}
			
			closesocket(top->sock);

			InetNtopA(AF_INET, &m_address, buffer, 256);
			ConsoleLog("Remove connection\t\t: [" << buffer << ":" << ntohs(top->address.sin6_port) << "]\n");

			m_resourceDeleter(&top->resource);

			SafeDelete(top);

			m_conns.erase(it);

			ConsoleLog("[" << buffer << "] remains\t\t: " << m_conns.size() << " connections\n");

		}

		m_trash.pop();
	}

	if (WaitToLock(m_incomingLock))
	{
		while (!m_incoming.empty())
		{
			auto top = m_incoming.front();

			auto it = m_conns.find(top->address.sin6_port);

			if (it == m_conns.end())
			{
				m_conns.insert({ top->address.sin6_port, top });
			}

			m_incoming.pop();
		}

		Release(m_incomingLock);
	}

	if (m_conns.empty())
	{
		m_isLoop = false;
		m_parent->Stop(this);
	}
	
}

bool ServerThread::Add(ClientConnection* conn)
{
	u_short port = conn->address.sin6_port;

	if (WaitToLock(m_incomingLock))
	{
		auto it = m_conns.find(port);

		if (it == m_conns.end())
		{
			conn->serv = this;
			//conn->servFunc = this->m_servFunc;

			m_incoming.push(conn);

			Release(m_incomingLock);
			return true;
		}

		Release(m_incomingLock);
	}

	return false;
}

ServerThread::~ServerThread()
{
	m_isLoop = false;

	Sleep(1000);

	for (auto& conn : m_conns)
	{
		SafeDelete(conn.second);
	}

	m_conns.clear();

	while (!m_incoming.empty())
	{
		auto conn = m_incoming.front();

		SafeDelete(conn);

		m_incoming.pop();
	}

	while (!m_trash.empty())
	{
		auto conn = m_trash.front();

		SafeDelete(conn);

		m_trash.pop();
	}

	SafeDelete(m_servThr);

}

ServerThread* Server::Listen(ClientResource* resource, void(*resourceDeleter)(ClientResource**))
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
		LogSSL();
		Throw(L"SSL_new() error.");
	}

	auto acc = SSL_accept(client.ssl);

	if (acc == 0)
	{
		/* Hard error */
		closesocket(client.sock);
		SSL_free(client.ssl);
		SafeDelete(c);
		resourceDeleter(&resource);
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
			resourceDeleter(&resource);
			return 0;
		}
		else if (err == SSL_ERROR_ZERO_RETURN)
		{
			closesocket(client.sock);
			SSL_clear(client.ssl);
			SafeDelete(c);
			resourceDeleter(&resource);
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
	std::cout << "Client request: " << buffer << ":" << ntohs(port) << "\n";

	c->resource = resource;

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

	std::cout << "Accept client: " << buffer << ":" << ntohs(port) << "\n";

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

void _Loop(ServerThread* serv)
{
	try
	{
		while (serv->m_isLoop)
		{
			for (auto it = serv->m_conns.begin();
				it != serv->m_conns.end(); it++)
			{
				serv->m_iter = it->second;
				if (serv->m_iter->ssl != 0 && serv->m_iter->bufferSize != 0)
				{
					serv->m_servFunc(serv->m_parent, serv, serv->m_iter, serv->m_iter->resource);
				}
				else
				{
					serv->m_parent->Close(serv->m_iter);
				}
			}
			serv->m_iter = nullptr;
			serv->GarbageCollect();
		}
	}
	catch (...)
	{
		serv->m_parent->ForceStop(serv);
	}

}
