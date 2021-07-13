#include "FileSharingHost.h"

#include "FileSharingServer.h"

#include <FileFolder.h>

#include "Connection.h"

#include "AdapterInfo.h"

#include "../Common/WindowsFileMapping.h"
#include "../Common/CommonFuction.h"

#include <sstream>
#include "FileEncryption.h"


__LocalConnection InitConnection()
{
    __LocalConnection conn;

    conn.sendName = L"Local\\FileSharingBuffer1";
    conn.recvName = L"Local\\FileSharingBuffer2";

    auto re = InitSend(conn);
    if (re != 0)
    {
        FreeConnection(conn);
        Throw(L"InitSend() error.");
    }

    std::string msg = "Hello world from host.";
    LocalSend(conn, msg.c_str(), msg.size());

    std::wcout << L"Wait for local connection ...\n";

#ifdef _DEBUG
    system("start D:\\20202\\Cryptography\\FileSharing\\FileSharing\\FileSharingClient\\x64\\Debug\\FileSharingClient.exe");
#else
    system("start FileSharingClient.exe");
#endif // _DEBUG

    while (conn.sendBuf[0] != 0)
    {
        Sleep(32);
    }

    re = InitRecv(conn);
    if (re != 0)
    {
        FreeConnection(conn);
        Throw(L"InitRecv() error.\n");
    }

    char* buffer = new char[BUF_SIZE]();
    LocalRecv(conn, buffer, BUF_SIZE);
    std::cout << buffer << "\n";
    delete[] buffer;

    return conn;
}

FileSharingHost::FileSharingHost()
{
//#ifndef _DEBUG
//    InitFSServer();
//#else
//    m_server = new FileSharingServer(L"");
//#endif // !_DEBUG

    wchar_t buffer[_MAX_DIR] = {};
    GetCurrentDirectoryW(_MAX_DIR, buffer);
    m_currentDir = buffer;

    InitFSServer();

    m_localConnection = new __LocalConnection();
    *m_localConnection = InitConnection();
    
    m_server->m_host = this;

    InitTask();
}

FileSharingHost::~FileSharingHost()
{
    SafeDelete(m_localConnection);
    SafeDelete(m_server);
}

void FileSharingHost::LaunchServer(FileSharingServer* server)
{
    server->Launch();
}

//if found, execute
//else try execute by m_server
std::wstring FileSharingHost::Execute(const std::wstring& cmd)
{
    std::wstring task = cmd.substr(0, cmd.find(L' '));

    if (task.empty()) return L"";
    ToLower(task);

    auto size = task.size();
    if (!m_suffix.empty()) task += L" " + m_suffix;

    auto it = m_clientTask.find(task);

    if (it != m_clientTask.end())
    {
        auto index = cmd.find_first_not_of(L' ', size + 1);
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
    else
    {
        return m_server->Execute(cmd);
    }
}

std::string FileSharingHost::InputPassword()
{
    StartSend(*m_localConnection);

    while (!ReadyToSend(*m_localConnection))
    {
        Sleep(16);
    }
    //if (ReadyToSend(*m_localConnection))
    //{
        ((wchar_t*)m_localConnection->sendBuf)[0] = WCHAR_MAX;
        ((wchar_t*)m_localConnection->sendBuf)[1] = WCHAR_MAX - 1;
    //}
    while (!ReadyToRecv(*m_localConnection))
    {
        Sleep(16);
    }
    
    std::string pass = m_localConnection->recvBuf;
    //m_localConnection->recvBuf[0] = 0;
    m_localConnection->sendBuf[0] = 0;
    //EndSend(*m_localConnection);
    ((wchar_t*)m_localConnection->recvBuf)[0] = 0;

    return pass;
}

void FileSharingHost::ChangeClientDir(const std::wstring& dir)
{
    StartSend(*m_localConnection);

    while (!ReadyToSend(*m_localConnection))
    {
        Sleep(16);
    }

    //if (ReadyToSend(*m_localConnection))
    //{
        ((wchar_t*)m_localConnection->sendBuf)[0] = WCHAR_MAX;
        ((wchar_t*)m_localConnection->sendBuf)[1] = WCHAR_MAX - 2;

        memcpy(&m_localConnection->sendBuf[4], (char*)&dir[0], dir.size() * 2 + 2);

    //}
    while (!ReadyToRecv(*m_localConnection))
    {
        Sleep(16);
    }
    //m_localConnection->recvBuf[0] = 0;
    m_localConnection->sendBuf[0] = 0;
    ((wchar_t*)m_localConnection->recvBuf)[0] = 0;
}

std::wstring FileSharingHost::QueryClient(const std::wstring& msg)
{
    StartSend(*m_localConnection);

    while (!ReadyToSend(*m_localConnection))
    {
        Sleep(16);
    }
    //if (ReadyToSend(*m_localConnection))
    //{
        ((wchar_t*)m_localConnection->sendBuf)[0] = WCHAR_MAX;
        ((wchar_t*)m_localConnection->sendBuf)[1] = WCHAR_MAX - 3;

        memcpy(&m_localConnection->sendBuf[4], (char*)&msg[0], msg.size() * 2 + 2);

    //}

    while (!ReadyToRecv(*m_localConnection))
    {
        Sleep(16);
    }

    std::wstring ret = (wchar_t*)&m_localConnection->recvBuf[0];
    //m_localConnection->recvBuf[0] = 0;
    m_localConnection->sendBuf[0] = 0;
    ((wchar_t*)m_localConnection->recvBuf)[0] = 0;
    return ret;
}

bool FileSharingHost::Confirm(const std::wstring& msg)
{
    auto ans = QueryClient(msg + L" (y/n): ");
    ToLower(ans);
    if (L"y" == ans || L"yes" == ans)
    {
        return 1;
    }
    return 0;
}

void FileSharingHost::SetClientPrefixAndUnit(const std::wstring& prefix, const std::wstring& unit)
{
    StartSend(*m_localConnection);

    while (!ReadyToSend(*m_localConnection))
    {
        Sleep(16);
    }
    
    memcpy(&m_localConnection->sendBuf[4], (char*)&prefix[0], prefix.size() * 2 + 2);
    
    memcpy(&m_localConnection->sendBuf[prefix.size() * 2 + 2 + 4], (char*)&unit[0], unit.size() * 2 + 2);

    ((wchar_t*)m_localConnection->sendBuf)[1] = WCHAR_MAX - 4;
    ((wchar_t*)m_localConnection->sendBuf)[0] = WCHAR_MAX;
}

size_t* FileSharingHost::GetClientTrackTotal()
{
    return (size_t*)&m_localConnection->sendBuf[4];
}

size_t* FileSharingHost::GetClientTrackCurrent()
{
    return (size_t*)&m_localConnection->sendBuf[12];
}

void FileSharingHost::StartClientTrack(const std::wstring& prefix, const std::wstring& unit, size_t** pCurrent, size_t** pTotal)
{
    SetClientPrefixAndUnit(prefix, unit);
    while (!ReadyToSend(*m_localConnection))
    {
        Sleep(16);
    }

    *pCurrent = GetClientTrackCurrent();
    *pTotal = GetClientTrackTotal();

    *(*pTotal) = 1;
    *(*pCurrent) = 0;

    ((wchar_t*)m_localConnection->sendBuf)[1] = WCHAR_MAX - 5;
    ((wchar_t*)m_localConnection->sendBuf)[0] = WCHAR_MAX;

    //std::wcout << L"((wchar_t*)m_localConnection->sendBuf)[0] = WCHAR_MAX\n";

    while (((wchar_t*)m_localConnection->recvBuf)[0] != WCHAR_MAX)
    {
        Sleep(8);
    }

    ((wchar_t*)m_localConnection->sendBuf)[1] = WCHAR_MAX - 5;

}

void FileSharingHost::EndClientTrack()
{
    ((wchar_t*)m_localConnection->sendBuf)[1] = 0;

    while (((wchar_t*)m_localConnection->recvBuf)[0] != 1)
    {
        Sleep(8);
    }

    ((wchar_t*)m_localConnection->sendBuf)[0] = 0;
    
    while (!ReadyToRecv(*m_localConnection))
    {
        Sleep(16);
    }

    ((wchar_t*)m_localConnection->recvBuf)[0] = 0;

}

void FileSharingHost::ClientDisplay(const std::wstring& msg)
{
    StartSend(*m_localConnection);

    while (!ReadyToSend(*m_localConnection))
    {
        Sleep(16);
    }
    
    ((wchar_t*)m_localConnection->sendBuf)[1] = WCHAR_MAX - 6;
    ((wchar_t*)m_localConnection->sendBuf)[0] = WCHAR_MAX;

    size_t sendBytes = 0;
    size_t totalBytes = msg.size() * 2 + 2;
    wchar_t* buff = (wchar_t*)m_localConnection->sendBuf;

    buff[2] = 0;
    ((wchar_t*)m_localConnection->sendBuf)[1] = WCHAR_MAX - 6;

    while (totalBytes - sendBytes >= BUF_SIZE - 4)
    {
        if (buff[2] != 0) Sleep(16);
        else
        {
            memcpy(&m_localConnection->sendBuf[4], (char*)&msg[0] + sendBytes, BUF_SIZE - 4);
            sendBytes += BUF_SIZE - 4;
        }
    }
    while (buff[2] != 0)
    {
        Sleep(16);
    }
    memcpy(&m_localConnection->sendBuf[4], (char*)&msg[0] + sendBytes, totalBytes - sendBytes);

}

std::vector<FileSharingHost::ServerIter> FileSharingHost::GetServerByName(const std::wstring& name)
{
    std::vector<ServerIter> ret;

    for (auto& it : m_sharedServ)
    {
        if (it.second.name == name)
        {
            ret.push_back(it);
        }
    }

    return ret;
}

FileSharingHost::ServerIter FileSharingHost::GetServerBySerial(size_t serial)
{
    size_t count = 0;
    for (auto& it : m_sharedServ)
    {
        if (count == serial)
        {
            return it;
        }
        count++;
    }

    Throw(L"Serial out of range.");
}

std::wstring FileSharingHost::Request(ServerIter& serv, const std::wstring& msg)
{
    return ::Request(serv.first, serv.second.sport, IPv4, msg);
}

#define FILE_TRANSFER_BUFFER_SIZE 4096

bool FileSharingHost::FileTransfer(std::fstream& fout, const std::wstring& param)
{
    Connection* conn = new Connection(m_curServ.first, m_curServ.second.sport, IPv4);

    std::wstring msg = L"transfer " + param;

    if (conn->Send((char*)&msg[0], msg.size() * 2 + 2) == -1)
    {
        delete conn;
        return false;
    }

    auto name = GetFileName(param);

    ULONG mode = 1;
    ioctlsocket(conn->m_sock, FIONBIO, &mode);

    char* buffer = new char[FILE_TRANSFER_BUFFER_SIZE + 8]();

    while (!IsReadyRead(conn->m_sock, 0, 16)) { Sleep(32); };

    auto re = conn->Recv(buffer, FILE_TRANSFER_BUFFER_SIZE);

    size_t* pTotal = 0;
    size_t* pCurrent = 0;

    StartClientTrack(L"Downloading file \"" + name + L"\" ... ", L"bytes ~", &pCurrent, &pTotal);

    auto& cur = *pCurrent;
    auto& total = *pTotal;

    cur = 0;

    if (re > 0)
    {
        //if first response is not "OK" return false;
        if (buffer[0] != 'O' || buffer[1] != 'K')
        {
            delete[] buffer;
            delete conn;
            EndClientTrack();
            return false;
        }
        else
        {
            total = *((size_t*)(buffer + 2));
        }
    }

    //auto limitWait = 20;

    while (true)
    {
        re = conn->Recv(buffer, FILE_TRANSFER_BUFFER_SIZE);
        if (re > 0)
        {
            fout.write(buffer, re);
            cur += re;
        }
        else if (re == 0) break;
        else if (re < 0)
        {
            //limitWait--;
            //if (limitWait == 0) break;
            if (re == CONNECTION_IO_RETURN::ERROR)
            {
                break;
            }
            Sleep(16);
        }
    }

    delete[] buffer;
    delete conn;

    EndClientTrack();

    return true;
}

bool FileSharingHost::FastFileTransfer(const std::wstring& path, const std::wstring& param)
{
    Connection* conn = new Connection(m_curServ.first, m_curServ.second.sport, IPv4);

    std::wstring msg = L"transfer " + param;

    if (conn->Send((char*)&msg[0], msg.size() * 2 + 2) == -1)
    {
        delete conn;
        return false;
    }

    auto name = GetFileName(param);

    ULONG mode = 1;
    ioctlsocket(conn->m_sock, FIONBIO, &mode);

    std::string buffer;
    buffer.resize(16);

    while (!IsReadyRead(conn->m_sock, 0, 16)) { Sleep(32); };

    auto re = conn->Recv(&buffer[0], 16);

    size_t* pTotal = 0;
    size_t* pCurrent = 0;

    StartClientTrack(L"Downloading file \"" + name + L"\" ... ", L"bytes ~", &pCurrent, &pTotal);

    auto& cur = *pCurrent;
    auto& total = *pTotal;

    cur = 0;

    if (re > 0)
    {
        //if first response is not "OK" return false;
        if (buffer[0] != 'O' || buffer[1] != 'K')
        {
            delete conn;
            EndClientTrack();
            return false;
        }
        else
        {
            total = *((size_t*)(&buffer[0] + 2));
        }
    }

    std::fstream fout;
    fout.rdbuf()->pubsetbuf(0, 0);
    fout.open(path, std::ios::binary | std::ios::out);
 
    size_t totalBytes = total;
    
    if (totalBytes > THRES_SIZE)
    {
        size_t recvBytes = 0;

        buffer.resize(BLOCK_SIZE);

        while (true)
        {
            re = conn->Recv(&buffer[0] + recvBytes, BLOCK_SIZE - recvBytes);
            if (re > 0)
            {
                cur += re;
                recvBytes += re;

                if (recvBytes == BLOCK_SIZE)
                {
                    fout.write(&buffer[0], BLOCK_SIZE);
                    totalBytes -= BLOCK_SIZE;
                    recvBytes = 0;                
                }
            }
            else if (re == 0) break;
            else if (re < 0)
            {
                if (re == CONNECTION_IO_RETURN::ERROR)
                {
                    break;
                }
                Sleep(16);
            }
        }

        fout.write(&buffer[0], totalBytes);

    }
    else
    {
        buffer.resize(totalBytes);

        while (true)
        {
            re = conn->Recv(&buffer[0] + cur, buffer.size() - cur);
            if (re > 0)
            {
                cur += re;
                if (cur == totalBytes) break;
            }
            else if (re == 0) break;
            else if (re < 0)
            {
                if (re == CONNECTION_IO_RETURN::ERROR)
                {
                    break;
                }
                Sleep(16);
            }
        }

        fout.write(&buffer[0], totalBytes);

    }

    delete conn;

    fout.close();

    EndClientTrack();

    return true;
}

void FileSharingHost::Launch()
{
    std::thread* servThr = new std::thread(LaunchServer, m_server);
    servThr->detach();

    m_server->UPDConnection()->SetMode(_NON_BLOCKING_MODE, 1);

    char* udpBuffer = new char[BUF_SIZE]();

    std::wstring udpMsg = L"FS_HOST " + m_server->m_name + L" " + std::to_wstring(m_server->m_server->m_port);
    m_server->UPDConnection()->SetRecvBuffer(udpBuffer, BUF_SIZE);
    m_server->UPDConnection()->SetMsg((char*)&udpMsg[0], udpMsg.size() * sizeof(wchar_t) + 2);

    char* buffer = new char[BUF_SIZE]();

    auto conn = m_server->UPDConnection();

    std::wstring response;
    std::wstring msg;

    while (true)
    {
        conn->Recv(
            [&](sockaddr_in6* src, int numbytes)
            {
                std::wstring recvMsg = (wchar_t*)&conn->m_recvBuffer[0];

                if (recvMsg.substr(0, 7) != L"FS_HOST")
                {
                    return;
                }

                auto ori = conn->GetDestinationHost();
                conn->SetDestinationHost(src);
                conn->Send(nullptr);
                conn->SetDestinationHost(&ori);
            });

        if (!ReadyToRecv(*m_localConnection)) Sleep(32);
        else
        {
            LocalRecv(*m_localConnection, buffer, BUF_SIZE);
            msg = (wchar_t*)buffer;

            Console::Begin();
            std::cout << "Local: ";
            //https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences#text-formatting
            Console::Set(TEXT_GREEN);
            std::wcout << msg << "\n";
            Console::SetWhiteText();
            Console::End();

            try
            {
                response = Execute(msg);
            }
            catch (const wchar_t* e)
            {
                response = e;
            }
            catch (const std::wstring e1)
            {
                response = e1;
            }
            catch (...)
            {
                response  = L"Unknown error.";
            }

            if (IsBlank(response)) response = L"Empty return from host.";

            size_t sendBytes = 0;
            size_t totalBytes = response.size() * 2 + 2;

            if (ReadyToSend(*m_localConnection))
            {
                StartSend(*m_localConnection);
                if (totalBytes <= BUF_SIZE)
                {
                    LocalSend(*m_localConnection, (char*)&response[0], totalBytes);
                }
                else
                {
                    while (totalBytes - sendBytes >= BUF_SIZE)
                    {
                        if (ReadyToSend(*m_localConnection))
                        {
                            LocalSend(*m_localConnection, ((char*)&response[0]) + sendBytes, BUF_SIZE);
                            sendBytes += BUF_SIZE;
                        }
                        else
                        {
                            Sleep(8);
                        }
                    }

                    auto remain = totalBytes - sendBytes;
                    if (remain > 0)
                    {
                        while (!ReadyToSend(*m_localConnection))
                        {
                            Sleep(8);
                        }

                        LocalSend(*m_localConnection, ((char*)&response[0]) + sendBytes, remain);
                    }
 
                }

                EndSend(*m_localConnection);

            }
            

        }
    }

    delete[] buffer;
    delete[] udpBuffer;

    SafeDelete(servThr);
}

void FileSharingHost::InitFSServer()
{
    std::wstring cmd;
    cmd.resize(256);

    auto list = GetAdaptersInformation();

    while (true)
    {
        std::wcout << "File sharing system: \n\t1. New session.\n\t2. Open from session file.\nChoose: ";
        ConsoleGetLine(&cmd[0], cmd.size());
        std::wcout << "\n";
        if (cmd[0] == L'1')
        {
            int count = 1;
            for (auto& adapter : list)
            {
                std::wcout << count << L". " << "[ADAPTER]\t\t" << adapter.desc << "\n   [NAME]\t\t"
                    << adapter.name << "\n   [IPv4]\t\t" << adapter.ipv4 << L"\n\n";
                count++;
            }

            std::wcout << "Choose your network: ";
            ConsoleGetLine(&cmd[0], cmd.size());

            if (IsNumber(cmd.c_str()))
            {
                auto index = std::stoi(cmd);
                index--;

                if (index < 0 || index >= list.size())
                {
                    std::wcout << L"Selection out of range.\n";
                    continue;
                }

                auto& adapter = list[index];

                std::wcout << "Sharing name: ";
                ConsoleGetLine(&cmd[0], cmd.size());

                std::wstring name = cmd.c_str();

                try
                {
                    m_server = new FileSharingServer(WStringToString(adapter.ipv4));
                    m_server->m_name = name;
                    std::wcout << "Start server successfully.\n";
                }
                catch (...)
                {
                    std::wcout << L"Another application is using port 443 of " << adapter.ipv4 << L"\n";
                    std::wcout << L"Enter server port: ";
                    ConsoleGetLine(&cmd[0], cmd.size());

                    if (IsNumber(cmd.c_str()))
                    {
                        auto port = std::stoi(cmd);

                        if (port < 1 || port > USHRT_MAX - 1)
                        {
                            std::wcout << L"Port must in range: 1 -> 65535.\n";
                            continue;
                        }

                        m_server = new FileSharingServer(WStringToString(adapter.ipv4), port);
                        m_server->m_name = name;
                    }
                    else
                    {
                        std::wcout << L"Port must be a number.\n";
                        continue;
                    }

                }
                m_server->m_udpSport = 8080;
                m_server->m_udpDport = 8080;
                
                break;
            }
            else
            {
                std::wcout << L"Selection must be a number.\n";
            }
            
        }
        else if (cmd[0] == L'2')
        {
            std::wcout << L"Path to session file: ";
            ConsoleGetLine(&cmd[0], cmd.size());
            std::wstring path = cmd.c_str();
            StandardPath(path);
            if (path.back() == L'\\') path.pop_back();

            if (!fs::is_regular_file(path))
            {
                std::wcout << L"\"" << path << L"\" is not a file.\n";
                continue;
            }

            auto pass = ::InputPassword();

            auto folder = m_currentDir;

            TempFile tempFile(folder);

            auto& fout = tempFile.WriteMode();

            std::fstream fin(path, std::ios::binary | std::ios::in);

            DecryptFile(fin, fout, pass);

            tempFile.ReadMode();

            try
            {
                m_server = new FileSharingServer(tempFile.m_path);
                std::wcout << L"Password correct. Open session file successfully.\n";
                m_oriSavePath = path;
            }
            catch (...)
            {
                std::wcout << L"Wrong password.\n";
                continue;
            }
            
            break;
        }
        else
        {
            std::wcout << L"Number out of range.\n";
        }
        std::wcout << "\n";
    }
    
    
}