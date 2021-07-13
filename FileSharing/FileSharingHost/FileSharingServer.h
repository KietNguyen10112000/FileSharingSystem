#pragma once

#include <map>
#include <string>
#include <functional>

#include "Server.h"
#include "UDPConnection.h"

#include <FolderToSingleFileEncryption.h>

namespace ef = FolderToSingleFileEncryption;

class FileSharingHost;
class FileSharingHostTask;
class FileSharingServerTask;

#define BLOCK_SIZE 64 * 1024 * 1024
#define THRES_SIZE 128 * 1024 * 1024

class FileSharingServer
{
private:
	friend class FileSharingHost;
	friend class FileSharingHostTask;
	friend class FileSharingServerTask;

	FileSharingHost* m_host = nullptr;

	std::map<std::wstring, std::wstring(*)(std::wstring, FileSharingServer*)> m_taskMap;

	Server* m_server = nullptr;

	UDPConnection* m_udpConn = nullptr;
	u_short m_udpSport = 0;
	u_short m_udpDport = 0;

	std::mutex m_efLock;
	//if m_efReady == true, execute request that access m_encryptedFolders
	//else return error string
	bool m_efReady = true;
	//encrypted folder to be share
	//critical resource if host access
	std::map<std::wstring, ef::EncryptedFolder*> m_encryptedFolders;

	std::mutex m_nfLock;
	//if m_nfReady == true, execute request that access m_normalFolders
	//else return error string
	bool m_nfReady = true;
	//normal folder to be share
	//critical resource if host access
	std::map<std::wstring, Folder<size_t>*> m_normalFolders;

	//std::string m_buffer;

	std::map<std::wstring, std::wstring> m_files;

public:
	std::wstring m_name;

public:
	FileSharingServer(const std::wstring& userFile = L"");
	FileSharingServer(const std::string& host, u_short sport = 443, u_short bport = 8080);
	~FileSharingServer();

private:
	void InitTaskMap();

	void InitUDPConnection(const std::string& host, u_short sport, u_short dport);

	void InitServer(const std::string& host, u_short port);

	void InitSharedEncryptedFolder(std::vector<std::wstring>& list);

	void InitSharedFolder(std::vector<std::wstring>& list);

	void LoadUserFile(const std::wstring& userFile);

	void LoadEncryptedFolder(std::wfstream& fin, int count);
	void LoadNormalFolder(std::wfstream& fin, int count);

public:
	auto& UPDConnection() { return m_udpConn; };
	std::string Address();

	std::wstring Execute(const std::wstring& cmd);

	wchar_t* Request(const std::wstring& cmd);

	template<typename _Func, typename ..._Args>
	void AddTask(_Func func, _Args&& ... args);

	void AddFolder(const std::wstring& path, std::string pass);
	void AddFolder(const std::wstring& path);
	void RemoveFolder(const std::wstring& name);

	ef::EncryptedFolder* GetEncryptedFolder(const std::wstring& name);
	Folder<size_t>* GetNormalFolder(const std::wstring& name);

	//normal file in storage
	//transfer a stream with size
	void Transfer(std::fstream& fin, size_t streamSize,
		ServerThread* servThread, ClientConnection* conn, ClientResource* resource);

	void EncryptedFileTransfer(ef::EncryptedFolder* enf, File<size_t>* file, size_t fileSize,
		ServerThread* servThread, ClientConnection* conn, ClientResource* resource);

	std::fstream* GetEncryptedFstream(const std::wstring& relativePath, std::string pass,
		ServerThread* servThread, ClientConnection* conn, ClientResource* resource);

	std::fstream* GetNormalFstream(const std::wstring& relativePath,
		ServerThread* servThread, ClientConnection* conn, ClientResource* resource);

	struct _Stream
	{
		std::fstream* f;
		bool isEf = false;
	};
	_Stream GetFstream(const std::wstring& param,
		ServerThread* servThread, ClientConnection* conn, ClientResource* resource);

public:
	void Save(const std::wstring& path);

	void Launch();

};

