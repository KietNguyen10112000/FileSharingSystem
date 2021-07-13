#include "FileSharingServer.h"

#include "FileSharingHost.h"

#include "../Common/File.h"

#include <sstream>
#include <FileFolder.h>

class FileSharingServerTask
{
private:
    friend class FileSharingServer;

private:
    static std::wstring HttpResponseDemo(std::wstring param, FileSharingServer* server);
    static std::wstring GetOverview(std::wstring param, FileSharingServer* server);

    static std::wstring AccessEncryptedFolder(std::wstring param, FileSharingServer* server);
    static std::wstring AccessNormalFolder(std::wstring param, FileSharingServer* server);

    static std::wstring RecvMsg(std::wstring param, FileSharingServer* server);
    //static std::wstring SendMsg(std::wstring param, FileSharingServer* server);

    static std::wstring PasswordRequest(std::wstring param, FileSharingServer* server);
    static std::wstring PasswordResponse(std::wstring param, FileSharingServer* server);

};

std::wstring FileSharingServerTask::HttpResponseDemo(std::wstring param, FileSharingServer* server)
{
    std::string response("<!doctype html><html><body><div><h1>Example Domain</h1></div></body></html>\n\0\0", 78);
    return (wchar_t*)&response[0];
}

std::wstring FileSharingServerTask::GetOverview(std::wstring param, FileSharingServer* server)
{
    std::wstring ret = L"Owner: " + server->m_name;

    ret += L"\n\nProtected folder: ";
    if (server->m_efReady)
    {
        if (server->m_encryptedFolders.empty()) ret += L"\n\t(Empty)";
        else
        {
            int count = 1;
            for (auto& e : server->m_encryptedFolders)
            {
                ret += L"\n\t" + std::to_wstring(count) + L". " + e.first;
                count++;
            }
        }
    }
    else
    {
        ret += L"\n\tNot avaiable right now.";
    }

    ret += L"\n\nPublic folder: ";
    if (server->m_nfReady)
    {
        if (server->m_normalFolders.empty()) ret += L"\n\t(Empty)";
        else
        {
            int count = 1;
            for (auto& e : server->m_normalFolders)
            {
                ret += L"\n\t" + std::to_wstring(count) + L". " + e.second->Name();
                count++;
            }
        }
    }
    else
    {
        ret += L"\n\tNot avaiable right now.";
    }

    return ret + L"\n\nResponse from: " + StringToWString(server->Address()) + L":" + std::to_wstring(server->m_server->m_port);
}

std::wstring FileSharingServerTask::AccessEncryptedFolder(std::wstring param, FileSharingServer* server)
{
    //param <password> <name>
    _Params params;
    ExtractParams(param, params);

    if (params.size() != 2) return L"Params incorrect.";

    auto pass = WStringToString(params[0]);
    auto& name = params[1];

    auto target = server->GetEncryptedFolder(name);

    if (!target) return L"nullptr";

    if (!target->VerifyKey(pass))
    {
        return L"Wrong password";
    }

    if (target == nullptr) return L"nullptr";

    return target->m_root->ToStructureString();
}

std::wstring FileSharingServerTask::AccessNormalFolder(std::wstring param, FileSharingServer* server)
{
    _Params params;
    ExtractParams(param, params);

    if (params.size() != 1) return L"Params incorrect.";

    auto& name = params[0];

    auto target = server->GetNormalFolder(name);

    if (target == nullptr) return L"nullptr";

    return target->ToStructureString();
}

std::wstring FileSharingServerTask::RecvMsg(std::wstring param, FileSharingServer* server)
{
    //msg <from addr> <from name> <msg> 
    auto id = param.find(L'|');
    std::wstring temp = param.substr(0, id);

    _Params params;
    ExtractParams(temp, params, 3);

    if (params.size() != 2) return L"nullptr";

    std::wstring msg = L"[" + params[0] + L"]\t\t\t: " + params[1] + L" says \"" + param.substr(id + 2) +L"\"";

    Console::BeginW();
    Console::Set(TEXT_YELLOW);
    std::wcout << "\033[1;38;2;0;255;242m";
    std::wcout << msg << L'\n';
    Console::SetWhiteText();
    Console::EndW();

    return L"OK";
}

std::wstring FileSharingServerTask::PasswordRequest(std::wstring param, FileSharingServer* server)
{
    //request-password <from address> <from name> <folder name>
    _Params params;
    ExtractParams(param, params);

    if (params.size() < 2) return L"nullptr";

    auto& addr = params[0];

    auto index = addr.find(L':');

    auto port = addr.substr(index + 1);

    std::wstring msg;
    std::wstring& folderName = params.back();

    auto f = server->GetEncryptedFolder(folderName);

    if (!f) return L"nullptr";

    //server->m_host->GetServerByName

    if (params.size() == 2)
    {
        msg = L"[" + addr.substr(0, index) + L"]\t\t\t: asks for the password of folder \"" + params[1] + L"\".";
    }
    else if (params.size() == 3)
    {
        msg = L"[" + addr.substr(0, index) + L"]\t\t\t: " + params[1] + L" asks for the password of folder \"" + params[2] + L"\".";
    }

    if (!IsNumber(port)) return L"nullptr";

    auto x = addr + L"|" + folderName;

    if (WaitToLock(server->m_host->m_passLock))
    {

        if (server->m_host->m_msg.find(x) != server->m_host->m_msg.end())
        {
            server->m_host->m_msg.insert({ x, msg });
        }

        Release(server->m_host->m_passLock);
    }

    Console::BeginW();
    Console::Set(TEXT_YELLOW);
    std::wcout 
        << L"\n─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────\n"
        << msg 
        << L"\n─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────\n\n";
    Console::SetWhiteText();
    Console::EndW();

    return L"Wait for owner give you the password of folder \"" + params.back() +L"\".";
}

std::wstring FileSharingServerTask::PasswordResponse(std::wstring param, FileSharingServer* server)
{
    //response-password <from address> <from name> <password> <folder name>

    _Params params;
    ExtractParams(param, params);

    if (params.size() != 4) return L"nullptr";

    auto first = params[3] + L"|" + params[0] + L"|" + params[1];
    auto& second = params[2];

    if (WaitToLock(server->m_host->m_passLock))
    {
        auto& map = server->m_host->m_recvPassword;
        if (map.find(first) == map.end())
        {
            map.insert({ first, second });
        }

        Release(server->m_host->m_passLock);
    }

    auto msg = L"[" + params[0] + L"]\t\t\t: " + params[1] + L" give you the password of folder \"" + params[3] + L"\".";

    Console::BeginW();
    //Console::Set(TEXT_YELLOW);
    std::cout << "\033[1;38;2;0;221;255m";
    std::wcout
        << L"\n─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────\n"
        << msg
        << L"\n─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────\n\n";
    Console::SetWhiteText();
    Console::EndW();

    return L"OK";
}

void FileSharingServer::InitTaskMap()
{
    m_taskMap.insert({ L"get", FileSharingServerTask::HttpResponseDemo });

    m_taskMap.insert({ L"get-overview", FileSharingServerTask::GetOverview });

    m_taskMap.insert({ L"access-ef", FileSharingServerTask::AccessEncryptedFolder });
    m_taskMap.insert({ L"access-nf", FileSharingServerTask::AccessNormalFolder });

    m_taskMap.insert({ L"request-password", FileSharingServerTask::PasswordRequest });
    m_taskMap.insert({ L"response-password", FileSharingServerTask::PasswordResponse });

    m_taskMap.insert({ L"msg", FileSharingServerTask::RecvMsg });
    
}