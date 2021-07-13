#include "FileSharingHost.h"

#include "FileSharingServer.h"

#include <sstream>
#include <FileFolder.h>

#include "Connection.h"

#include "../Common/WindowsFileMapping.h"
#include "../Common/CommonFuction.h"

#include <sstream>
#include "FileEncryption.h"

#include <io.h>



class FileSharingHostTask
{
private:
    friend class FileSharingHost;

private:
    static std::wstring ChangeDirectory(std::wstring param, FileSharingHost* host);
    static std::wstring Scan(std::wstring param, FileSharingHost* host);
    static std::wstring ShowSharedServ(std::wstring param, FileSharingHost* host);
    static std::wstring Share(std::wstring param, FileSharingHost* host);

    static std::wstring ShowSharedFolder(std::wstring param, FileSharingHost* host);

    static std::wstring SaveSession(std::wstring param, FileSharingHost* host);

    static std::wstring AskPassword(std::wstring param, FileSharingHost* host);
    static std::wstring GivePassword(std::wstring param, FileSharingHost* host);
    static std::wstring ShowPassword(std::wstring param, FileSharingHost* host);

    //request
    static std::wstring GetOverview(std::wstring param, FileSharingHost* host);

    //file sharing access, request
    static std::wstring Goto(std::wstring param, FileSharingHost* host);

    static std::wstring GotoFSA(std::wstring param, FileSharingHost* host);
    static std::wstring LeaveFSA(std::wstring param, FileSharingHost* host);

    static std::wstring AcessFolder(std::wstring param, FileSharingHost* host);

    static std::wstring PullFile(const std::wstring& from, const std::wstring& to, FileSharingHost* host);
    static std::wstring PullFolder(const std::wstring& from, const std::wstring& to, 
        Folder<size_t>* folder, FileSharingHost* host);
    static std::wstring Pull(std::wstring param, FileSharingHost* host);

    static std::wstring GetFSA(std::wstring param, FileSharingHost* host);

    static std::wstring ChangeDirectoryFSA(std::wstring param, FileSharingHost* host);
    static std::wstring ShowFSA(std::wstring param, FileSharingHost* host);

    static std::wstring CloseFSA(std::wstring param, FileSharingHost* host);


    static std::wstring TestTrackProcess(std::wstring param, FileSharingHost* host);

    static std::wstring SendMsg(std::wstring param, FileSharingHost* host);

};

std::wstring FileSharingHostTask::ChangeDirectory(std::wstring param, FileSharingHost* host)
{
    if (fs::is_directory(param))
    {
        host->m_currentDir = CombinePath(host->m_currentDir, param);

        if (param.back() == '\\')
        {
            host->m_currentDir.pop_back();
        }
        SetCurrentDir(host->m_currentDir);
    }
    else
    {
        return L"\"" + param + L"\" is not a directory or not exist !";
    }
    return L"Directory changed to \"" + host->m_currentDir + L"\".";
}

std::wstring FileSharingHostTask::Scan(std::wstring param, FileSharingHost* host)
{
    auto conn = host->m_server->UPDConnection();

    size_t i = 0;

    host->m_sharedServ.clear();

    conn->Loop(
        [&]()
        {
            i++;
            if (i == 10) conn->Stop();
            Sleep(200);
        },
        [&](sockaddr_in6* src, int numbytes)
        {
            if (std::wstring((wchar_t*)&conn->m_recvBuffer[0], 7) != L"FS_HOST")
            {
                return;
            }

            std::string srcDesc;
            srcDesc.resize(256);

            InetNtopA(AF_INET, &((u_short*)src)[2], &srcDesc[0], srcDesc.size());

            srcDesc = srcDesc.c_str();

            std::string& hostSrc = srcDesc;//(char*)&srcDesc[0];

//#ifndef _DEBUG
//            if (hostSrc == host->m_server->Address()) return;
//#endif // !_DEBUG

            std::wstring name = (wchar_t*)&conn->m_recvBuffer[0];
            name = name.substr(name.find(L' ') + 1);

            auto i = name.find_last_of(L' ');
            u_short sport = std::stoi(name.substr(i + 1));

            name = name.substr(0, i);

            //hostSrc += ":" + std::to_string(ntohs(src->sin6_port));

            auto it = host->m_sharedServ.find(hostSrc);

            if (it == host->m_sharedServ.end())
            {
                host->m_sharedServ.insert({ hostSrc, { name, sport, ntohs(src->sin6_port) } });
            }

        }
        );
    
    return ShowSharedServ(L"", host);
}

std::wstring FileSharingHostTask::ShowSharedServ(std::wstring param, FileSharingHost* host)
{
    std::wstring ret = L"\tAddress\t\t\tSport\t\tBport\t\tName";

    int count = 1;

    for (auto& it : host->m_sharedServ)
    {
        ret += L"\n───────────────────────────────────────────────────────────────────────────────────────────────\n"
            + std::to_wstring(count) + L"\t"
            + StringToWString(it.first) + L"\t\t" + std::to_wstring(it.second.sport) + L"\t\t"
            + std::to_wstring(it.second.bport) + L"\t\t" + it.second.name;
        count++;
    }
    ret += L"\n───────────────────────────────────────────────────────────────────────────────────────────────";
    return ret;
}

std::wstring FileSharingHostTask::Share(std::wstring param, FileSharingHost* host)
{
    if(param.back() == L'\\') param.pop_back();

    if (fs::is_directory(param))
    {
        host->m_server->AddFolder(CombinePath(host->m_currentDir, param));
    }
    else
    {
        auto pass = host->InputPassword();
        host->m_server->AddFolder(CombinePath(host->m_currentDir, param), pass);
    }

    return L"Folder \"" + GetFileName(param) + L"\" shared.";
}

std::wstring FileSharingHostTask::ShowSharedFolder(std::wstring param, FileSharingHost* host)
{
    return host->m_server->Execute(L"get-overview ");
}

std::wstring FileSharingHostTask::SaveSession(std::wstring param, FileSharingHost* host)
{
    std::wstring path;
    std::wstring folderPath;
    if (IsBlank(param))
    {
        folderPath = host->m_currentDir;
        if (host->m_oriSavePath.empty())
            path += host->m_currentDir + L'\\' + host->m_server->m_name + L"_session_" + std::to_wstring(GetTickCount64());
        else
            path = host->m_oriSavePath;
    }
    else
    {
        folderPath = CombinePath(host->m_currentDir, param);
        if (fs::is_directory(folderPath))
        {
            path += folderPath + L'\\' + host->m_server->m_name + L"_session_" + std::to_wstring(GetTickCount64());
        }
        else
        {
            path = folderPath;
            folderPath = folderPath.substr(0, folderPath.find_last_of(L'\\'));
        }
    }

    TempFile tempFile(folderPath);

    host->m_server->Save(tempFile.m_path);

    auto& fin = tempFile.ReadMode();

    std::fstream fout(path, std::ios::binary | std::ios::out);


    host->ClientDisplay(L"\033[1;33mPassword is required to save current session:\033[0m\n");
    auto pass = host->InputPassword();

    EncryptFile(fin, fout, pass);

    return L"Session file saved to \"" + path + L"\".";
}

std::wstring FileSharingHostTask::AskPassword(std::wstring param, FileSharingHost* host)
{
    if (param.empty())  return L"ask-password <option> <param>\n\t<option>\t: :s (require if use serial number)\n\t<param>\t: name or serial number";

    std::wstring request = L"request-password \"" + StringToWString(host->m_server->m_server->m_host) 
        + L":" + std::to_wstring(host->m_server->m_server->m_port) +
        + L"\" \"" + host->m_server->m_name + L"\"";

    std::wstring prefix = L"Protected folder:";

    std::wstring name = param;

    if (name.empty()) return L"Name/serial can not empty.";

    //int serial = -1;
    auto iS = param.find(L":s");
    std::wstring serialStr;

    if (iS != std::wstring::npos)
    {
        serialStr = param.substr(param.find_first_not_of(L' ', iS + 2));

        if (!IsNumber(serialStr))
        {
            return L"Serial must be a number.";
        }
    }

    if (!serialStr.empty())
    {
        auto nfIndex = host->m_overviewDesc.find(prefix);

        auto start = host->m_overviewDesc.find(serialStr, nfIndex);

        if (start == std::wstring::npos) return L"Serial out of range.";

        auto end = host->m_overviewDesc.find(L'\n', start + serialStr.size());

        name = host->m_overviewDesc.substr(start + serialStr.size() + 2, end - start - serialStr.size() - 2);
    }

    if (!name.empty())
    {
        request += name + L"\"";

        host->m_isNormalFolder = false;
    }

    return ::Request(host->m_curServ.first, host->m_curServ.second.sport, IPv4, request) 
        + L"\nUse \"show-pw\" to show the password.";
}

std::wstring FileSharingHostTask::GivePassword(std::wstring param, FileSharingHost* host)
{
    //give-pw <of folder> <to>
    //give-pw Repo :s 1
    //give-pw Repo name

    auto pass = host->InputPassword();

    std::wstring request = L"response-password \"" + StringToWString(host->m_server->m_server->m_host)
        + L"\" \"" + host->m_server->m_name + L"\" \"" + StringToWString(pass) + L"\" \"";

    auto index = param.find(L":s");

    std::wstring folderName;
    FileSharingHost::ServerIter s;

    if (index != std::wstring::npos)
    {
        auto pos = param.find_last_of(L" ", index + 1, 1);
        folderName = param.substr(0, pos);
        while (folderName.back() == L' ')
        {
            folderName.pop_back();
        }

        auto serialStr = param.substr(param.find_first_not_of(L' ', index + 2));

        if (!IsNumber(serialStr))
        {
            return L"Serial must be a number.";
        }

        auto serial = std::stoi(serialStr);
        s = host->GetServerBySerial(serial - 1);
    }
    else
    {
        _Params params;
        ExtractParams(param, params, 2);

        if (params.size() == 1) return L"give-pw <of folder> <option> <param>\n\t\
<of folder>\t: name of a protected folder\n\t<option>\t: :s (can be ignore) \n\t<param>\t\t : name/serial";

        folderName = params[0];
        auto list = host->GetServerByName(params[1]);

        if (list.size() != 0)
        {
            return L"User name not found.";
        }
        if (list.size() != 1)
        {
            return L"There are one more than name found. Try to use serial intends.";
        }

        s = list[0];
    }

    auto folder = host->m_server->GetEncryptedFolder(folderName);

    if (!folder) return L"Folder \"" + folderName + L"\" not found.";

    request += folderName + L"\"";

    auto respones = ::Request(s.first, s.second.sport, IPv4, request);

    return L"Gave the password of folder \"" + folderName + L"\" to user \"" + s.second.name + L"\".";
}

std::wstring FileSharingHostTask::ShowPassword(std::wstring param, FileSharingHost* host)
{
    //show-pw :s 1
    //show-pw :all

    //auto all = param.find(L":all");
    auto sr = param.find(L":s");
    std::wstring ret;

    auto parse = [](std::wstring& str, size_t& offset) -> std::wstring
    {
        auto index = str.find(L'|', offset);
        auto re = str.substr(offset, index - offset);
        offset = index + 1;
        return re;
    };

    std::wstring tempStr;
    if (sr == std::wstring::npos)
    {
        if (WaitToLock(host->m_passLock))
        {
            for (auto& pass : host->m_recvPassword)
            {
                size_t temp = 0;
                tempStr = pass.first;

                auto name = parse(tempStr, temp);
                auto addr = parse(tempStr, temp);
                auto username = parse(tempStr, temp);
                
                ret += L"[" + addr + L"]: " + username + L"    [Folder]: " + name + L"    [Password]: " + pass.second + L"\n";
            }
            Release(host->m_passLock);
        }
    }
    else
    {
        auto serialStr = param.substr(param.find_first_not_of(L' ', sr + 2));

        if (!IsNumber(serialStr))
        {
            return L"Serial must be a number.";
        }

        auto serial = std::stoi(serialStr);
        if (WaitToLock(host->m_passLock))
        {
            auto count = 1;
            for (auto& pass : host->m_recvPassword)
            {
                if (count == serial)
                {
                    size_t temp = 0;
                    tempStr = pass.first;

                    auto name = parse(tempStr, temp);
                    auto addr = parse(tempStr, temp);
                    auto username = parse(tempStr, temp);

                    ret += L"[" + addr + L"]: " + username + L"    [Folder]: " + name + L"    [Password]: " + pass.second + L"\n";
                    break;
                }
                count++;
            }
            Release(host->m_passLock);
        }
    }

    return ret + L"\nUse \"system cls\" to clear screen after you remember passwords.";
}

std::wstring FileSharingHostTask::GetOverview(std::wstring param, FileSharingHost* host)
{
    //get <name>

    //get by serial
    //get :s <number>

    if(param.empty())  return L"info <option> <param>\n\t<option>\t: :s (require if use serial number)\n\t<param>\t: name or serial number";


    std::wstring request = L"get-overview ";

    auto index = param.find(L":s");

    if (index != std::wstring::npos)
    {
        std::wstring serialStr = param.substr(param.find_first_not_of(L' ', index + 2));

        int serial = 0;

        if (IsNumber(serialStr))
        {
            serial = std::stoi(serialStr);
            auto s = host->GetServerBySerial(serial - 1);

            auto re = ::Request(s.first, s.second.sport, IPv4, request);
            return re;
        }
        else
        {
            return L"Serial must be a number.";
        }

    }
    else
    {
        std::wstring ret;

        auto list = host->GetServerByName(param);

        if (list.size() == 0) return L"Name not found.";

        for (auto& serv : list)
        {
            ret += L"\n\n" + ::Request(serv.first, serv.second.sport, IPv4, request);
        }

        return ret;
    }

}

std::wstring FileSharingHostTask::Goto(std::wstring param, FileSharingHost* host)
{
    //goto <name>

    //goto by serial
    //goto :s <number>

    if (param.empty())  return L"goto <option> <param>\n\t<option>\t: :s (require if use serial number)\n\t<param>\t: name or serial number";

    auto index = param.find(L":s");

    if (index != std::wstring::npos)
    {
        std::wstring serialStr = param.substr(param.find_first_not_of(L' ', index + 2));

        int serial = 0;

        if (IsNumber(serialStr))
        {
            serial = std::stoi(serialStr);
            auto s = host->GetServerBySerial(serial - 1);
            host->m_fsaCurrentDir = L"FS:\\" + s.second.name;
            host->ChangeClientDir(host->m_fsaCurrentDir);
            host->m_curServ = s;
        }
        else
        {
            return L"Serial must be a number.";
        }

    }
    else
    {
        std::wstring ret;

        auto list = host->GetServerByName(param);

        if (list.size() == 0) return L"Name not found.";

        if (list.size() != 1)
        {
            return L"There are one more than name found. Try to use serial intends.";
        }
        else
        {
            host->m_fsaCurrentDir = L"FS:\\" + list[0].second.name;
            host->ChangeClientDir(host->m_fsaCurrentDir);
            host->m_curServ = list[0];
        }
    }

    //direct to file sharing access
    host->m_suffix = L"-fsa";

    host->m_overviewDesc = ::Request(host->m_curServ.first, host->m_curServ.second.sport, IPv4, L"get-overview ");

    return L"Goto successfully.";
}

std::wstring FileSharingHostTask::GotoFSA(std::wstring param, FileSharingHost* host)
{
    return L"Use \"leave\" to leave of current sharing folder first.";
}

std::wstring FileSharingHostTask::LeaveFSA(std::wstring param, FileSharingHost* host)
{
    if (host->Confirm(L"Leave \"" + host->m_curServ.second.name + L"\" ?"))
    {
        std::wstring ret;
        if (host->m_root)
        {
            ret = CloseFSA(L"", host) + L'\n';
        }

        auto name = host->m_curServ.second.name;

        host->m_curServ = {};
        host->m_fsaCurrentDir = L"";
        host->m_overviewDesc = L"";
        host->m_suffix = L"";
        host->ChangeClientDir(host->m_currentDir);

        return ret + L"Left \"" + name + L"\".";
    }

    return L"\033[2A";
}

std::wstring FileSharingHostTask::AcessFolder(std::wstring param, FileSharingHost* host)
{
    std::wstring prefix;

    auto index = param.find(L":nf");
    auto index2 = param.find(L":ef");
    std::wstring name;

    if (index != std::wstring::npos)
    {
        prefix = L"Public folder:";
        name = param.substr(param.find_first_not_of(L' ', index + 3));
    }
    else if (index2 != std::wstring::npos)
    {
        prefix = L"Protected folder:";
        name = param.substr(param.find_first_not_of(L' ', index2 + 3));
    }
    else
    {
        return L"access <type> <option> <param>:\n\t<type>\t: :nf (normal folder)/:ef (protected folder)."
            "\n\t<option>\t: :s (use serial).\n\t<param>\t: name or serial number.";
    }

    if (name.empty()) return L"Name/serial can not empty.";

    //int serial = -1;
    auto iS = param.find(L":s");
    std::wstring serialStr;

    if (iS != std::wstring::npos)
    {
        serialStr = param.substr(param.find_first_not_of(L' ', iS + 2));

        if (!IsNumber(serialStr))
        {
            return L"Serial must be a number.";
        }
    }
    
    if (!serialStr.empty())
    {
        auto nfIndex = host->m_overviewDesc.find(prefix);

        auto start = host->m_overviewDesc.find(serialStr, nfIndex);

        if (start == std::wstring::npos) return L"Serial out of range.";

        auto end = host->m_overviewDesc.find(L'\n', start + serialStr.size());

        name = host->m_overviewDesc.substr(start + serialStr.size() + 2, end - start - serialStr.size() - 2);
    }

    std::wstring request;

    if (index != std::wstring::npos)
    {
        if (!name.empty())
        {
            request = L"access-nf \"" + name + L"\"";
            host->m_isNormalFolder = true;
        }
    }
    else if (index2 != std::wstring::npos)
    {
        if (!name.empty())
        {
            std::wstring pass = StringToWString(host->InputPassword());
            request = L"access-ef \"" + pass + L"\" \"" + name + L"\"";
            host->m_isNormalFolder = false;
        }
    }

    auto response = ::Request(host->m_curServ.first, host->m_curServ.second.sport, IPv4, request);

    if (response != L"nullptr")
    {
        bool wrongPass = false;

        if (index2 != std::wstring::npos)
        {
            wrongPass = response == L"Wrong password";
        }

        if (!wrongPass)
        {
            if (host->m_root)
            {
                host->m_root->RecursiveFree();
                SafeDelete(host->m_root);
            }
            host->m_root = new Folder<size_t>();

            std::wstringstream ss(response);
            host->m_root->LoadStructure(ss);

            *host->m_root->m_iteratorPath = host->m_fsaCurrentDir + L'\\' + host->m_root->Name();

            host->ChangeClientDir(*host->m_root->m_iteratorPath);

            host->m_fsaCurrentDir = *host->m_root->m_iteratorPath;

            Console::Begin(ULONG_MAX);

            std::wstringstream buffer;
            std::wstreambuf* old = std::wcout.rdbuf(buffer.rdbuf());
            host->m_root->PrettyPrint();
            std::wcout.rdbuf(old);

            Console::End();
            return buffer.str();
        }
        else
        {
            return L"Wrong password.";
        }
    }
    else
    {
        return L"Folder not found.";
    }
    
    return L"Access nothing."; 
}

std::wstring FileSharingHostTask::PullFile(const std::wstring& from, const std::wstring& to, FileSharingHost* host)
{
    if (host->m_isNormalFolder)
    {
        /*std::fstream fout(to, std::ios::binary | std::ios::out);

        if (host->FileTransfer(fout, L":n " + from))
        {
            return L"File \"" + from + L" saved to \"" + to + L"\".";
        }*/
        if (host->FastFileTransfer(to, L":n " + from))
        {
            return L"File \"" + from + L" saved to \"" + to + L"\".";
        }
    }
    else
    {
        auto pass = host->InputPassword();

        auto wpass = StringToWString(pass);

        auto folder = to.substr(0, to.find_last_of(L'\\'));

        pass.resize(32);

        TempFile tempFile(folder);

        auto& fstream = tempFile.WriteMode();


        if (host->FileTransfer(fstream, L'\"' + wpass + L"\" \"" + from + L'\"'))
        {
            std::fstream fout(to, std::ios::binary | std::ios::out);

            tempFile.ReadMode();
            DecryptFile(fstream, fout, pass);

            fout.close();

            return L"File \"" + GetFileName(to) + L"\" saved to \"" + folder + L"\".";
        }

    }

    return L"Can not pull file to local.";
 
}

std::wstring FileSharingHostTask::PullFolder(const std::wstring& from, const std::wstring& to, 
    Folder<size_t>* folder, FileSharingHost* host)
{
    folder->MoveTo(to);

    auto rootName = folder->Name();
    auto prefix = from.substr(0, from.find_last_of(L'\\'));

    size_t fileCount = 0;
    size_t folderCount = 0;

    if (host->m_isNormalFolder)
    {
        folder->Traverse(
            [&](Folder<size_t>* current)
            {
                CreateNewDirectory(current->Path());
                folderCount++;
                for (auto& f : current->Files())
                {
                    auto path = f->Path();
                    //std::fstream fout(path, std::ios::binary | std::ios::out);

                    auto rPath = path.substr(path.find(rootName));

                    //rPath = prefix + rPath;

                    /*if (host->FileTransfer(fout, L":n " + rPath))
                    {
                        fileCount++;
                    }*/

                    if (host->FastFileTransfer(path, L":n " + rPath))
                    {
                        fileCount++;
                    }

                }
            },
            nullptr
        );
    }
    else
    {
        auto pass = host->InputPassword();

        auto wpass = StringToWString(pass);

        pass.resize(32);

        TempFile tempFile(to);

        folder->Traverse(
            [&](Folder<size_t>* current)
            {
                CreateNewDirectory(current->Path());
                folderCount++;
                for (auto& f : current->Files())
                {
                    auto path = f->Path();

                    auto rPath = path.substr(path.find(rootName));
                    rPath = prefix + L'\\' + rPath;

                    auto& fstream = tempFile.WriteMode();

                    if (host->FileTransfer(fstream, L'\"' + wpass + L"\" \"" + rPath + L'\"'))
                    {
                        std::fstream fout(path, std::ios::binary | std::ios::out);

                        tempFile.ReadMode();
                        DecryptFile(fstream, fout, pass);

                        fout.close();

                        fileCount++;
                    }
                }
            },
            nullptr
            );
    }

    if (folder != host->m_root)
    {
        SafeDelete(folder->m_iteratorPath);
    }

    if (fileCount != 0 || folderCount != 0)
    {
        return L"Total " + std::to_wstring(fileCount) + L" files, " + std::to_wstring(folderCount) + L" folders pulled to local.";
    }

    return L"Can not pull folder to local.";
}

std::wstring FileSharingHostTask::Pull(std::wstring param, FileSharingHost* host)
{
    if (!host->m_root) return L"Access to a folder to pull.";

    _Params params;
    ExtractParams(param, params, 2);

    std::wstring savePath;
    std::wstring pullpath;

    if (params.size() == 1)
    {
        savePath = params[0];
        pullpath = host->m_fsaCurrentDir;
    }
    else if (params.size() == 2)
    {
        savePath = params[1];
        pullpath = CombinePath(host->m_fsaCurrentDir, params[0]);
    }
    else
    {
        savePath = host->m_currentDir;
        pullpath = host->m_root->m_name;
        //return L"pull <from> <to>:\n\t<from>\t: relative path to a file/folder in the sharing folder.\n\t<to>\t: path to folder in local storage.";
    }

    if (!fs::is_directory(savePath)) return L"\"" + savePath + L"\" is not a directory.";

    //auto& pullPath = params[0];

    if (savePath.back() == L'\\') savePath.pop_back();

    auto index = pullpath.find(host->m_root->m_name);

    pullpath = pullpath.substr(index);

    auto folder = host->m_root->GetFolder(pullpath);
    auto file = host->m_root->GetFile(pullpath);

    if (file)
    {
        savePath += L'\\' + GetFileName(pullpath);
        return PullFile(pullpath, savePath, host);
    }

    if (folder)
    {
        return PullFolder(pullpath, savePath, folder, host);
    }

    return L"File/Folder not found.";
}

std::wstring FileSharingHostTask::GetFSA(std::wstring param, FileSharingHost* host)
{
    return host->m_overviewDesc;
}

std::wstring FileSharingHostTask::ChangeDirectoryFSA(std::wstring param, FileSharingHost* host)
{
    bool isRelPath = IsValidRelativePath(param);
    
    if (!isRelPath)
    {
        return L"Change directory in sharing folder support for \"Relative path\" only.";
    }
    else
    {
        auto path = CombinePath(host->m_fsaCurrentDir, param);

        if (path.back() == L'\\') path.pop_back();

        auto index = path.find(host->m_curServ.second.name);

        if (index == std::wstring::npos)
        {
            host->ChangeClientDir(host->m_fsaCurrentDir);
            return L"Use \"leave\" to leave the current sharing folder.";
        }

        if (!host->m_root->GetFolder(param))
        {
            return L"Folder not found.";
        }

        index = path.find(host->m_root->Name());

        if (index == std::wstring::npos)
        {
            host->ChangeClientDir(host->m_fsaCurrentDir);
            return L"Use \"close\" to close the current accessing to sharing folder.";
        }

        host->ChangeClientDir(path);
        host->m_fsaCurrentDir = path;

        return L"\033[2A";
    }
}

std::wstring FileSharingHostTask::ShowFSA(std::wstring param, FileSharingHost* host)
{
    auto folder = host->m_root->GetFolder(MakeRelativePath(L"FS:\\" + host->m_curServ.second.name, host->m_fsaCurrentDir));

    if (folder)
    {
        Console::BeginW(ULONG_MAX);

        std::wstringstream buffer;
        std::wstreambuf* old = std::wcout.rdbuf(buffer.rdbuf());
        folder->PrettyPrint();
        std::wcout.rdbuf(old);

        Console::EndW();

        return buffer.str();
    }
    else
    {
        return host->m_overviewDesc;
    }
   
}

std::wstring FileSharingHostTask::CloseFSA(std::wstring param, FileSharingHost* host)
{
    if (!host->m_root) return LeaveFSA(param, host);

    if (host->Confirm(L"Close \"" + host->m_root->Name() + L"\" ?"))
    {
        host->m_fsaCurrentDir = L"FS:\\" + host->m_curServ.second.name;

        host->ChangeClientDir(host->m_fsaCurrentDir);

        auto name = host->m_root->Name();

        host->m_root->RecursiveFree();

        SafeDelete(host->m_root);

        return L"Closed accessing to \"" + name + L"\".";
    }

    return L"\033[2A";
}

std::wstring FileSharingHostTask::TestTrackProcess(std::wstring param, FileSharingHost* host)
{
    size_t* pTotal = 0;
    size_t* pCurrent = 0;

    host->StartClientTrack(L"Downloading file ... ", L"bytes ~", &pCurrent, &pTotal);

    *pTotal = 300;
    *pCurrent = 0;

    auto& current = *pCurrent;

    while (current != *pTotal)
    {
        current++;
        Sleep(32);
    }

    host->EndClientTrack();

    return L"Done.";
}

std::wstring FileSharingHostTask::SendMsg(std::wstring param, FileSharingHost* host)
{
    auto sr = param.find(L":s");

    std::wstring request = L"msg \"" + StringToWString(host->m_server->m_server->m_host) + L"\" \""
        + host->m_server->m_name + L"\" | ";

    FileSharingHost::ServerIter serv;

    if (sr == std::wstring::npos)
    {
        if (host->m_curServ.first.empty())
        {
            //send <option> <to> <msg>
            Throw(L"send <option> <to> <msg>\n\t<option>\t: :s\n\t<to>\t\t: name/serial of host\n\t<msg>\t\t: message");
        }
        else
        {
            request += param;
            serv = host->m_curServ;
        }
    }
    else
    {
        auto id = param.find_first_not_of(L' ', sr + 2);
        auto id2 = param.find_first_of(L' ', id);
        auto serialStr = param.substr(id, id2 - id);

        if (!IsNumber(serialStr))
        {
            return L"Serial must be a number.";
        }

        auto serial = std::stoi(serialStr);

        serv = host->GetServerBySerial(serial - 1);

        request += param.substr(param.find_first_not_of(L' ', id2));

    }

    //request += L"\"";
    
    auto res = ::Request(serv.first, serv.second.sport, IPv4, request);

    return L"Msg send to " + serv.second.name +L" at [" 
        + StringToWString(serv.first) + L":" + std::to_wstring(serv.second.sport) + L"].";
}


void FileSharingHost::InitTask()
{
    m_clientTask.insert({ L"quit", [](std::wstring param, FileSharingHost* host)->std::wstring { exit(0); return L"Quited."; } });

    m_clientTask.insert({ L"cd", FileSharingHostTask::ChangeDirectory });
    m_clientTask.insert({ L"scan", FileSharingHostTask::Scan });
    m_clientTask.insert({ L"show", FileSharingHostTask::ShowSharedServ });
    m_clientTask.insert({ L"share", FileSharingHostTask::Share });

    m_clientTask.insert({ L"my-sharing", FileSharingHostTask::ShowSharedFolder });

    m_clientTask.insert({ L"save", FileSharingHostTask::SaveSession });

    //request
    m_clientTask.insert({ L"info", FileSharingHostTask::GetOverview });
    
    //goto file sharing access
    m_clientTask.insert({ L"goto", FileSharingHostTask::Goto });

    m_clientTask.insert({ L"my-sharing -fsa", FileSharingHostTask::ShowSharedFolder });
    m_clientTask.insert({ L"save -fsa", FileSharingHostTask::SaveSession });

    //file sharing access
    m_clientTask.insert({ L"info -fsa", FileSharingHostTask::GetFSA });
    m_clientTask.insert({ L"goto -fsa", FileSharingHostTask::GotoFSA });
    m_clientTask.insert({ L"leave -fsa", FileSharingHostTask::LeaveFSA });

    m_clientTask.insert({ L"access -fsa", FileSharingHostTask::AcessFolder });

    m_clientTask.insert({ L"pull -fsa", FileSharingHostTask::Pull });

    m_clientTask.insert({ L"close -fsa", FileSharingHostTask::CloseFSA });

    m_clientTask.insert({ L"cd -fsa", FileSharingHostTask::ChangeDirectoryFSA });
    m_clientTask.insert({ L"show -fsa", FileSharingHostTask::ShowFSA });

    m_clientTask.insert({ L"show-fss", FileSharingHostTask::ShowSharedServ });

    m_clientTask.insert({ L"ask-pw -fsa", FileSharingHostTask::AskPassword });
    m_clientTask.insert({ L"give-pw -fsa", FileSharingHostTask::GivePassword });
    m_clientTask.insert({ L"give-pw", FileSharingHostTask::GivePassword });
    m_clientTask.insert({ L"show-pw", FileSharingHostTask::ShowPassword });
    m_clientTask.insert({ L"show-pw -fsa", FileSharingHostTask::ShowPassword });

    m_clientTask.insert({ L"test", FileSharingHostTask::TestTrackProcess });

    m_clientTask.insert({ L"send", FileSharingHostTask::SendMsg });
    m_clientTask.insert({ L"send -fsa", FileSharingHostTask::SendMsg });

}