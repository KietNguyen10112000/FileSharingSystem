#include "FileSharingServer.h"

#include "FileSharingHost.h"

#include "../Common/File.h"

#include <FileFolder.h>

FileSharingServer::FileSharingServer(const std::wstring& userFile)
{
	InitTaskMap();
//#ifdef _DEBUG
//	std::wstring tUserFile = L"D:\\20202\\Cryptography\\FileSharing\\Resource\\User info\\User1.txt";
//	LoadUserFile(tUserFile);
//#else
//	if (!userFile.empty())
//	{
//		LoadUserFile(userFile);
//	}
//#endif // _DEBUG
	LoadUserFile(userFile);

	sockaddr_in6 addr;
	int len = sizeof(sockaddr_in6);

	std::string msg = "haha";
	m_udpConn->SetMsg(&msg[0], msg.size());
	m_udpConn->SetMode(_NON_BLOCKING_MODE, 1);

	m_udpConn->SetRecvBuffer(&msg[0], msg.size());

	std::string srcDesc;

	m_udpConn->Loop(
		[&]()
		{
			Sleep(16);
			m_udpConn->Stop();
		}, 
		[&](sockaddr_in6* src, int numbytes)
		{
			srcDesc.resize(256);
			InetNtopA(AF_INET, &((u_short*)src)[2], &srcDesc[0], srcDesc.size());
			srcDesc = srcDesc.c_str();
		});

	if(srcDesc != m_server->m_host)
		m_server->m_host = srcDesc;
}

FileSharingServer::FileSharingServer(const std::string& host, u_short sport, u_short bport)
{
	InitTaskMap();
	try
	{
		m_server = new Server(host, sport, SERVER_IPv4);
		m_udpConn = new UDPConnection(host, bport, "192.168.1.255", bport);
	}
	catch (...)
	{
		SafeDelete(m_udpConn);
		SafeDelete(m_server);
		throw;
	}
	
}

FileSharingServer::~FileSharingServer()
{
	SafeDelete(m_server);
	SafeDelete(m_udpConn);

	for (auto& f : m_encryptedFolders)
	{
		SafeDelete(f.second);
	}

	for (auto& f : m_normalFolders)
	{
		SafeDelete(f.second);
	}

}


void FileSharingServer::InitUDPConnection(const std::string& host, u_short sport, u_short dport)
{
	m_udpConn = new UDPConnection(sport, dport);
	m_udpSport = sport;
	m_udpDport = dport;
}

void FileSharingServer::InitServer(const std::string& host, u_short port)
{
	m_server = new Server(host, port, SERVER_IPv4);
}

void FileSharingServer::LoadUserFile(const std::wstring& userFile)
{
	std::wfstream f(userFile, std::ios::binary | std::ios::in);

	std::wstring line;
	auto lo = f.imbue(std::locale("en_US.utf8"));
	std::string host;
	
	std::getline(f, line);
	if (line.empty()) Throw(L"xxxx");
	if (line.back() == L'\r') line.pop_back();
	if (line != L"//FILE_SHARING_USER_INFO")
	{
		Throw(L"File format not correct.");
	}

	while (!f.eof())
	{
		std::getline(f, line);
		if (line.size() < 2 || (line[0] == L'/' && line[1] == L'/')) continue;

		size_t index = line.find_last_of(L']');

		if (index != std::wstring::npos)
		{
			std::wstring var = line.substr(1, index - 1);

			auto i = line.find_last_of(L':');
			std::wstring value = line.substr(i + 2);

			if (value.back() == L'\r') value.pop_back();

			if (var == L"Address")
			{
				//value.pop_back();
				host = WStringToString(value);
			}
			else if (var == L"Bport")
			{
				auto i1 = value.find(L',');

				u_short sport = std::stoi(value.substr(0, i1));
				u_short dport = std::stoi(value.substr(i1 + 1));

				InitUDPConnection(host, sport, dport);

			}
			else if (var == L"Sport")
			{
				u_short Sport = std::stoi(value);
				InitServer(host, Sport);
			}
			else if (var == L"Name")
			{
				m_name = value;
				//m_name.pop_back();
			}
			else if (var == L"Encrypted")
			{
				int count = std::stoi(value);
				LoadEncryptedFolder(f, count);
			}
			else
			{
				int count = std::stoi(value);
				LoadNormalFolder(f, count);
			}

		}

	}

	f.close();
}

void FileSharingServer::LoadEncryptedFolder(std::wfstream& fin, int count)
{
	std::wstring line;
	for (size_t i = 0; i < count; i++)
	{
		std::getline(fin, line);
		if (line.back() == L'\r') line.pop_back();

		auto index = line.find(L'|');
		auto path = line.substr(0, index - 1);
		auto pass = WStringToString(line.substr(index + 2));

		auto enf = ef::OpenEncryptedFolder(path, pass);
		m_encryptedFolders.insert({ enf->m_root->Name(), enf });

	}
}

void FileSharingServer::LoadNormalFolder(std::wfstream& fin, int count)
{
	std::wstring line;
	for (size_t i = 0; i < count; i++)
	{
		std::getline(fin, line);
		if (line.back() == L'\r') line.pop_back();

		auto folder = new Folder<size_t>(line.c_str());
		m_normalFolders.insert({ folder->Name(), folder });

	}
}

std::string FileSharingServer::Address()
{
	return m_server->m_host;
}

std::wstring FileSharingServer::Execute(const std::wstring& cmd)
{
	std::wstring task = cmd.substr(0, cmd.find(L' '));

	if (task.empty()) return L"";
	ToLower(task);

	auto it = m_taskMap.find(task);

	if (it != m_taskMap.end())
	{
		auto index = cmd.find_first_not_of(L' ', task.size() + 1);
		if (index == std::wstring::npos)
		{
			return it->second(L" ", this);
		}
		else
		{
			auto param = cmd.substr(index);
			StandardPath(param);
			return it->second(param, this);
		}
	}

	return L"\"" + task +L"\" command not found.";
}

void FileSharingServer::AddFolder(const std::wstring& path, std::string pass)
{
	m_efReady = false;
	if (WaitToLock(m_efLock))
	{
		try
		{
			ef::EncryptedFolder* enf = ef::OpenEncryptedFolder(path, pass);

			auto name = enf->m_root->Name();

			auto it = m_encryptedFolders.find(name);

			if (it == m_encryptedFolders.end())
			{
				m_encryptedFolders.insert({ name, enf });
			}
			else
			{
				Release(m_efLock);
				m_efReady = true;
				SafeDelete(enf);
				Throw(L"Folder \"" + name + L" already shared.");
			}
		}
		catch (const wchar_t* e)
		{
			Release(m_efLock);
			throw e;
		}
		/*catch (...)
		{
			Release(m_efLock);
			Throw(L"Unknown error.");
		}*/
		Release(m_efLock);
	}
	m_efReady = true;
}

void FileSharingServer::AddFolder(const std::wstring& path)
{
	m_nfReady = false;
	if (WaitToLock(m_nfLock))
	{
		try
		{
			auto name = GetFileName(path);

			auto it = m_normalFolders.find(name);

			if (it == m_normalFolders.end())
			{
				Folder<size_t>* folder = new Folder<size_t>(path.c_str());
				m_normalFolders.insert({ name, folder });
			}
			else
			{
				Release(m_nfLock);
				m_nfReady = true;
				Throw(L"Folder \"" + name + L" already shared.");
			}
		}
		catch (const wchar_t* e)
		{
			Release(m_nfLock);
			throw e;
		}
		/*catch (...)
		{
			Release(m_efLock);
			Throw(L"Unknown error.");
		}*/
		Release(m_nfLock);
	}
	m_nfReady = true;
}

ef::EncryptedFolder* FileSharingServer::GetEncryptedFolder(const std::wstring& name)
{
	if (m_efReady)
	{
		auto it = m_encryptedFolders.find(name);

		if (it != m_encryptedFolders.end())
		{
			return it->second;
		}
	}

	return nullptr;

}

Folder<size_t>* FileSharingServer::GetNormalFolder(const std::wstring& name)
{
	if (m_efReady)
	{
		auto it = m_normalFolders.find(name);

		if (it != m_normalFolders.end())
		{
			return it->second;
		}
	}

	return nullptr;
}

#define MAX_WAIT_LOOP 10
#define TIMEOUT 3000

struct _ExtendResource : public ClientResource
{
	bool endRequest = false;

	unsigned long long sendBytes = 0;

	int maxWait = MAX_WAIT_LOOP;

	union
	{
		unsigned long long start;
		unsigned long long totalBytes;
	} var1;
	
	union
	{
		unsigned long long current = 0;
		ef::EncryptedFolder* enf;
	} var2;

	unsigned long long timeout = TIMEOUT;

	union
	{
		std::fstream* stream = nullptr;
		File<size_t>* file;
	} var3;

	_ExtendResource()
	{
		var1.start = GetTickCount64();
	}
	~_ExtendResource() { SafeDelete(var3.stream); };
};

void FileSharingServer::Transfer(std::fstream& fin, size_t streamSize,
	ServerThread* servThread, ClientConnection* conn, ClientResource* resource)
{
	auto resrc = (_ExtendResource*)resource;

	if (resrc->var2.current)
	{
		//per block transfer

		if (resrc->sendBytes == BLOCK_SIZE)
		{
			if (resrc->var1.totalBytes >= BLOCK_SIZE)
			{
				fin.read(&resrc->response[0], BLOCK_SIZE);
				//resrc->var1.totalBytes -= BLOCK_SIZE;
				//resrc->response.resize(BLOCK_SIZE);
			}
			else
			{
				fin.read(&resrc->response[0], resrc->var1.totalBytes);
				//resrc->var1.totalBytes = 0;
				resrc->response.resize(resrc->var1.totalBytes);
			}
			resrc->sendBytes = 0;
		}

		if (IsReadyWrite(conn->sock, 0, 16))
		{
			auto re = conn->Send(((char*)&resrc->response[0]) + resrc->sendBytes,
				resrc->response.size() - resrc->sendBytes);
			if (re == -1)
			{
				m_server->Close(conn);
			}
			else if (re > 0)
			{
				resrc->sendBytes += re;
				resrc->var1.totalBytes -= re;
			}
		}

		if (resrc->var1.totalBytes == 0)
		{
			m_server->Close(conn);
		}

	}
	else
	{
		if (resrc->response.size() != streamSize)
		{
			resrc->response.resize(streamSize);
			fin.read(&resrc->response[0], streamSize);
		}

		if (IsReadyWrite(conn->sock, 0, 16))
		{
			auto re = conn->Send(((char*)&resrc->response[0]) + resrc->sendBytes,
				resrc->response.size() - resrc->sendBytes);
			if (re == -1)
			{
				m_server->Close(conn);
			}
			else if (re > 0)
			{
				resrc->sendBytes += re;

				if (resrc->sendBytes == streamSize)
				{
					m_server->Close(conn);
				}

			}

		}
	}
}

#define EncryptedFileTransfer_BUFFER_SIZE 4096

void FileSharingServer::EncryptedFileTransfer(
	ef::EncryptedFolder* enf, File<size_t>* file, size_t fileSize,
	ServerThread* servThread, ClientConnection* conn, ClientResource* resource)
{
	auto resrc = (_ExtendResource*)resource;

	if (fileSize > THRES_SIZE)
	{
		//per block transfer
	}
	else
	{

		if (resrc->response.size() != fileSize)
		{
			resrc->response.resize(fileSize);
			enf->Read(resrc->var3.file, &resrc->response[0]);
			resrc->var3.file = nullptr;
		}

		if (IsReadyWrite(conn->sock, 0, 16))
		{
			auto re = conn->Send(((char*)&resrc->response[0]) + resrc->sendBytes,
				resrc->response.size() - resrc->sendBytes);
			if (re == -1)
			{
				m_server->Close(conn);
			}
			else if (re > 0)
			{
				resrc->sendBytes += re;

				if (resrc->sendBytes == fileSize)
				{
					m_server->Close(conn);
				}

			}

		}
		
	}

}

std::fstream* FileSharingServer::GetEncryptedFstream(const std::wstring& relativePath, std::string pass, 
	ServerThread* servThread, ClientConnection* conn, ClientResource* resource)
{
	if (!m_efReady) return nullptr;

	auto index = relativePath.find_first_of(L'\\');

	if (index == std::wstring::npos) return nullptr;

	auto name = relativePath.substr(0, index);
	auto path = relativePath.substr(index + 1);

	auto folder = GetEncryptedFolder(name);

	if (!folder) return nullptr;

	auto file = folder->m_root->GetFile(path);

	if (!file) return nullptr;

	auto filePath = file->Path();

	((_ExtendResource*)resource)->var1.totalBytes = folder->GetSize(file);
	((_ExtendResource*)resource)->var2.enf = folder;
	((_ExtendResource*)resource)->var3.file = file;

	return folder->m_fstream;
}

std::fstream* FileSharingServer::GetNormalFstream(const std::wstring& relativePath,
	ServerThread* servThread, ClientConnection* conn, ClientResource* resource)
{
	if (!m_nfReady) return nullptr;

	auto index = relativePath.find_first_of(L'\\');

	if (index == std::wstring::npos) return nullptr;

	auto name = relativePath.substr(0, index);
	auto path = relativePath.substr(index + 1);

	auto folder = GetNormalFolder(name);

	if (!folder) return nullptr;

	auto file = folder->GetFile(path);

	if (!file) return nullptr;

	auto filePath = file->Path();

	//fs::file_size(filePath);

	if (!fs::exists(filePath)) return nullptr;

	auto totalBytes = fs::file_size(filePath);

	((_ExtendResource*)resource)->var1.totalBytes = totalBytes;

	if (totalBytes > THRES_SIZE)
	{
		((_ExtendResource*)resource)->sendBytes = BLOCK_SIZE;
		resource->response.resize(BLOCK_SIZE);
		((_ExtendResource*)resource)->var2.current = 1;
	}
	else
	{
		((_ExtendResource*)resource)->var2.current = 0;
	}

	return new std::fstream(filePath, std::ios::binary | std::ios::in);
}

FileSharingServer::_Stream FileSharingServer::GetFstream(const std::wstring& param,
	ServerThread* servThread, ClientConnection* conn, ClientResource* resource)
{
	//if found, this is normal file transfer
	auto n = param.find(L":n");

	if (n != std::wstring::npos)
	{
		return { GetNormalFstream(param.substr(param.find_first_not_of(L' ', n + 2)), servThread, conn, resource), false };
	}
	else
	{
		_Params params;
		std::wstring temp = param;
		ExtractParams(temp, params);

		if (params.size() != 2) return {};

		return { GetEncryptedFstream(params[1], WStringToString(params[0]), servThread, conn, resource), true };
	}

}

void FileSharingServer::Save(const std::wstring& path)
{
	std::wfstream fout(path, std::ios::binary | std::ios::out);

	auto lo = fout.imbue(std::locale("en_US.utf8"));

	fout << L"//FILE_SHARING_USER_INFO\n[Address]\t: ";

	fout << StringToWString(m_server->m_host)
		<< L"\n[Bport]\t\t: " << std::to_wstring(m_udpSport) << L", " << std::to_wstring(m_udpDport)
		<< L"\n[Sport]\t\t: " << std::to_wstring(m_server->m_port)
		<< L"\n[Name]\t\t: " << m_name
		<< L"\n\n[Normal]\t\t: " << m_normalFolders.size() << L"\n";

	for (auto& f : m_normalFolders)
	{
		fout << f.second->Path() << L"\n";
	}

	fout << L"\n[Encrypted]\t\t: " << m_encryptedFolders.size() << L"\n";

	for (auto& f : m_encryptedFolders)
	{
		if (m_host->Confirm(L"Save \"" + f.second->m_name + L"\" ? "))
		{
			auto pass = m_host->InputPassword();
			fout << f.second->m_path << L" | " << StringToWString(pass) << L"\n";
		}
	}

	fout.flush();
	fout.close();
}

void FileSharingServer::Launch()
{
	//std::string msg = "<!doctype html><html><body><div><h1>Example Domain</h1></div></body></html>";
	static FileSharingServer* cur = this;
	m_server->Listen<_ExtendResource>(
		[](Server* server, ServerThread* servThread, ClientConnection* conn, ClientResource* resource)
		{
			//auto conn = *pConn;
			//auto& resource = conn->m_resource;

			auto resrc = (_ExtendResource*)resource;

			if (!resrc->endRequest)
			{
				if (IsReadyRead(conn->sock, 0, 16))
				{
					auto re = conn->Recv(&conn->buffer[0], conn->bufferSize);

					if (re == -1)
					{
						server->Close(conn);
						return;
					}
					else if (re > 0)
					{
						if (re < 2)
						{
							server->Close(conn);
							return;
						}
						else
						{
							resource->request.append(&conn->buffer[0], re);
						}

						if (conn->buffer[re - 1] == '\0' && conn->buffer[re - 2] == '\0')
						{
							//end of msg
							resrc->endRequest = true;
							resrc->maxWait == 0;
						}

					}
				}
				else
				{
					resrc->maxWait--;
					if (resrc->maxWait == 0)
					{
						resrc->endRequest = true;
					}
				}
			}
			

			if (resrc->endRequest)
			{
				if (resrc->response.empty())
				{
					std::wstring wresquest;
					std::string opt = resrc->request.substr(0, 3);
					ToLower(opt);
					//simulate http response
					if(opt == "get") wresquest = StringToWString(resrc->request);
					else if (opt == "sto")
					{
						//set timeout to reach the end of IO connection to do something like file transfer, ...
						if (resrc->timeout == TIMEOUT)
						{
							auto newTO = *(uint64_t*)&resrc->request[3];
							resrc->timeout = newTO == TIMEOUT ? TIMEOUT + 1 : newTO;
						}
					}
					else
					{
						resrc->request.push_back('\0');
						resrc->request.push_back('\0');
						wresquest = (wchar_t*)&resrc->request[0];
					}
					if (wresquest.substr(0, 8) == L"transfer")
					{
						//mark as transfer
						resrc->maxWait = -1;

						//set timeout to end of transfer
						resrc->timeout = ULLONG_MAX;

						//get transfer stream
						auto trs = cur->GetFstream(wresquest.substr(wresquest.find_first_not_of(L' ', 8)),
							servThread, conn, resource);

						if (trs.isEf)
						{
							resrc->sendBytes = 1;
						}
						else
						{
							resrc->var3.stream = trs.f;
						}
					}
					else
					{
						std::wstring res = cur->Execute(wresquest);
						resrc->response = std::string((char*)&res[0], (char*)&res[res.size()]);
						resrc->maxWait = 0;
					}
				}

				if (resrc->maxWait != 0)
				{
					if (resrc->maxWait == -1)
					{
						if (!resrc->var3.stream)
						{
							resrc->response = "nullptr";
							resrc->maxWait = 0;
							resrc->timeout = TIMEOUT;

							conn->Send(((char*)&resrc->response[0]),
								resrc->response.size());

							Sleep(300);

							server->Close(conn);
						}
						else
						{
							size_t oriSize = resrc->response.size();
							resrc->response.resize(10);
							resrc->response[0] = 'O';
							resrc->response[1] = 'K';
							//resrc->response.resize(2 + 8);
							::memcpy(&resrc->response[2], &resrc->var1.totalBytes, 8);

							if (resrc->sendBytes == 1)
							{
								resrc->maxWait = -3;
								resrc->sendBytes = 0;
							}
							else resrc->maxWait = -2;

							conn->Send(((char*)&resrc->response[0]), 10);

							resrc->response.resize(oriSize);
						}
						
					}
					if (resrc->maxWait == -2)
					{
						//file transfer
						cur->Transfer(*resrc->var3.stream, resrc->var1.totalBytes, servThread, conn, resource);
					}
					else if (resrc->maxWait == -3)
					{
						//file transfer
						cur->EncryptedFileTransfer(resrc->var2.enf, resrc->var3.file, resrc->var1.totalBytes, 
							servThread, conn, resource);
					}
				}
				else
				{
					if (IsReadyWrite(conn->sock, 0, 16))
					{
						auto re = conn->Send(((char*)&resrc->response[0]) + resrc->sendBytes,
							resrc->response.size() - resrc->sendBytes);
						if (re == -1)
						{
							server->Close(conn);
						}
						else if (re > 0)
						{
							resrc->sendBytes += re;

							if (resrc->sendBytes == resrc->response.size())
							{
								server->Close(conn);
							}

						}

					}
					else
					{
						resrc->maxWait++;
						if (resrc->maxWait == MAX_WAIT_LOOP)
						{
							server->Close(conn);
						}
					}
				}
			}

			if (resrc->timeout != ULLONG_MAX)
			{
				resrc->var2.current = GetTickCount64();

				if (resrc->var2.current - resrc->var1.start > resrc->timeout)
				{
					server->Close(conn);
				}
			}

		});
}
