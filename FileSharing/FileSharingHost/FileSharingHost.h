#pragma once

#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <queue>

class FileSharingServer;
class FileSharingHostTask;
class FileSharingServerTask;
template<typename T> class Folder;

struct __LocalConnection;

//host for local client
class FileSharingHost
{
private:
	friend class FileSharingServer;
	friend class FileSharingServerTask;
	friend class FileSharingHostTask;

	FileSharingServer* m_server;
	__LocalConnection* m_localConnection;

	struct _FSServer
	{
		std::wstring name;
		unsigned short sport;
		unsigned short bport;
	};

	//address, name, Sport, ...
	std::map<std::string, _FSServer> m_sharedServ;

	using ServerIter = std::pair<std::string, _FSServer>;

	std::map<std::wstring, std::wstring(*)(std::wstring, FileSharingHost*)> m_clientTask;

	std::wstring m_oriSavePath;

	std::wstring m_currentDir;

	std::wstring m_suffix = L"";


	//for file sharing access
	ServerIter m_curServ;

	bool m_isNormalFolder = false;
	Folder<size_t>* m_root = nullptr;
	std::wstring m_fsaCurrentDir;

	std::wstring m_overviewDesc;
	//std::set<std::wstring> m_fsaEncryptedList;
	//std::set<std::wstring> m_fsaNormalList;

	std::mutex m_passLock;
	//folder name | address | ower name, pass
	std::map<std::wstring, std::wstring> m_recvPassword;

	std::mutex m_msgLock;
	//address, msg
	std::map<std::wstring, std::wstring> m_msg;

public:
	FileSharingHost();
	~FileSharingHost();

private:
	void InitTask();

	static void LaunchServer(FileSharingServer*);

	std::wstring Execute(const std::wstring& cmd);

	std::string InputPassword();
	void ChangeClientDir(const std::wstring& dir);
	std::wstring QueryClient(const std::wstring& msg);
	bool Confirm(const std::wstring& msg);

	void SetClientPrefixAndUnit(const std::wstring& prefix, const std::wstring& unit);
	size_t* GetClientTrackTotal();
	size_t* GetClientTrackCurrent();
	void StartClientTrack(const std::wstring& prefix, const std::wstring& unit, size_t** pCurrent, size_t** pTotal);
	void EndClientTrack();

	void ClientDisplay(const std::wstring& msg);

	std::vector<ServerIter> GetServerByName(const std::wstring & name);
	ServerIter GetServerBySerial(size_t serial);

	std::wstring Request(ServerIter& serv, const std::wstring& msg);

	//void ParseOverview(const std::wstring& msg);

	//return true on success
	//param can be ":n <path>" (for normal folder) or "<password> <path>" (for encrypted forder)
	bool FileTransfer(std::fstream& fout, const std::wstring& param);

	bool FastFileTransfer(const std::wstring& path, const std::wstring& param);

public:
	void InitFSServer();

	void Launch();

};

