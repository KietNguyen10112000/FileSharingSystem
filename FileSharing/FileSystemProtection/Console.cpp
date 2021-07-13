#include <Windows.h>

#include <string>
#include <iostream>
#include <sstream>
#include <map>

#include <conio.h>

#include "FileFolder.h"

#include "../Common/CommonFuction.h"
#include "../Common/File.h"

#include "FileSystemProtectionAPI.h"

#include "FolderToSingleFileEncryptionTask.h"





#define ConsoleFunction void(*)(std::wstring)

std::map<std::wstring, ConsoleFunction> taskMap;

std::wstring _taskSuffix = L"";

std::wstring currentDir;

namespace ef = FolderToSingleFileEncryption;
ef::EncryptedFolder* curEncryptedFolder = nullptr;

void ShowFolderTree(std::wstring param)
{
    //StandardPath(param);
    Folder<int>* folder = new Folder<int>(CombinePath(currentDir, param).c_str());

    std::wcout << folder << '\n';

    folder->RecursiveFree();
    delete folder;
}

void PrintStructureFile(std::wstring param)
{
    //StandardPath(param);
    _Params list;
    ExtractParams(param, list);

    Folder<int>* folder = new Folder<int>(CombinePath(currentDir, list[0]).c_str(), FOLDER_FLAG::LOAD_STRUCTURE);

    std::wcout << folder << '\n';

    folder->RecursiveFree();
    delete folder;
}

void SaveFolderStructure(std::wstring param)
{
    _Params list;
    ExtractParams(param, list);

    if (list.size() == 0)
    {
        Throw(L"Params must be <Folder> <Path> in which:\n\t<Folder>\t: a folder in storage.\n\t<Path>\t\t: path to save file.");
    }

    auto opt = list[0];
    ToLower(opt);

    if (opt == L"relative")
    {
        list.erase(list.begin());
    }

    if (list.size() == 1)
    {
        StandardPath(list[0]);
        Folder<int>* folder = new Folder<int>(currentDir.c_str());

        if (opt == L"relative")
        {
            folder->ToRelativePath();
        }

        folder->SaveStructure(CombinePath(currentDir, list[0]).c_str());
        
        delete folder;
        return;
    }

    if (list.size() != 2)
    {
        Throw(L"Params must be <Folder> <Path> in which:\n\t<Folder>\t: a folder in storage.\n\t<Path>\t\t: path to save file.");
    }

    StandardPath(list[0]);
    Folder<int>* folder = new Folder<int>(CombinePath(currentDir, list[0]).c_str());

    StandardPath(list[1]);
    folder->SaveStructure(CombinePath(currentDir, list[1]).c_str());
    delete folder;
}

void ChangeDirectory(std::wstring param)
{
    //StandardPath(param);

    if (fs::is_directory(param))
    {
        currentDir = CombinePath(currentDir, param);

        if (param.back() == '\\')
        {
            currentDir.pop_back();
        }
        SetCurrentDir(currentDir);
    }
    else
    {
        Throw(L"\"" + param + L"\" is not a directory or not exist !");
    }
    
}

void EncryptFolder(std::wstring path, std::wstring savePath, std::string key)
{
    //StandardPath(path);
    //StandardPath(savePath);
    Folder<std::string>* folder = new Folder<std::string>(path.c_str());

    EncryptFolder(folder, savePath, key);

}

void EncryptFile_(const std::wstring& path)
{

}

void _EncryptFolderToFile(std::wstring path, std::wstring savePath, std::string key)
{
    //StandardPath(path);
    //StandardPath(savePath);
    //Folder<_ExtraData>* folder = new Folder<_ExtraData>(path.c_str());

    //EncryptFolderToFile(folder, savePath, key);
    Folder<size_t>* folder = new Folder<size_t>(path.c_str());
    auto encryptedFolder = FolderToSingleFileEncryption::EncryptFolder(&folder, savePath, key);
    delete encryptedFolder;
    if(folder) delete folder;
}

void EncryptFolderTask(std::wstring param)
{
    _Params list;
    ExtractParams(param, list, 2);

    bool toFile = false;
    for (size_t i = 0; i < list.size(); i++)
    {
        if (list[i] == L"-ef")
        {
            list.erase(list.begin() + i);
            toFile = true;
            break;
        }
    }

    

    if (toFile)
    {
        if (list.size() == 0 || list.size() > 2)
        {
            Throw(L"Params must be <Folder> <Path> in which:\n\t\
<Folder>\t: a folder in storage.\n\t<Path>\t\t: path to save the encrypted folder (optional).");
        }

        auto name = GetFileName(list[0]);
        auto pass = InputPassword();

        if (list.size() == 1)
        {
            auto path = currentDir + L'\\' + name + L".ef";
            _EncryptFolderToFile(list[0], path, pass);
            std::wcout << "Encrypted folder to \"" << path << L"\".\n";
        }

        if (list.size() == 2)
        {
            if (!fs::is_directory(list[1]))
            {
                auto path = CombinePath(currentDir, list[1]);

                if (path.find(L".ef") == std::wstring::npos) path += L".ef";

                _EncryptFolderToFile(list[0], path, pass);
                std::wcout << "Encrypted folder to \"" << path << L"\".\n";
            }
            else
            {
                auto path = CombinePath(currentDir, list[1]) + L'\\' + name + L".ef";
                _EncryptFolderToFile(list[0], path, pass);
                std::wcout << "Encrypted folder to \"" << path << L"\".\n";
            }
               
        }
    }
    else
    {
        if (list.size() < 2 || list.size() > 3)
        {
            Throw(L"Params must be <Password> <Folder> <Path> in which:\n\t\
<Folder>\t: a folder in storage.\n\t<Path>\t\t: path to save the encrypted folder (optional).");
        }

        auto name = GetFileName(list[1]);
        if (list.size() == 2)
        {
            auto path = currentDir + L'\\' + name + L"_Encrypted";
            EncryptFolder(list[1], path, WStringToString(list[0]));
            std::wcout << "Encrypted folder to \"" << path << L"\".\n";
        }
        else if (list.size() == 3)
        {
            if (!fs::is_directory(list[2]))
            {
                Throw(L"\"" + list[2] + L"\" is not a directory.");
            }
            EncryptFolder(list[1], CombinePath(currentDir, list[2]) + L'\\' + name + L"_Encrypted", WStringToString(list[0]));
        }
    }

}

void OpenEncryptedFolderTask(std::wstring param)
{
    _Params list;
    ExtractParams(param, list, 2);
    if (list.size() == 1)
    {
        try
        {
            SafeDelete(curEncryptedFolder);
            curEncryptedFolder = ef::Open(list[0]);
            currentDir = list[0];

            auto path = currentDir.substr(0, currentDir.find_last_of(L'\\'));
            SetCurrentDir(path);

            _taskSuffix = L"-ef";
            std::wcout << L"Password correct. Open encrypted folder successfully.\n";
        }
        catch (...)
        {
            throw L"Wrong password.";
        }
    }
    else
    {
        Throw(L"Params must be <Path> in which:\n\t<Path>\t\t: path to the encrypted folder.");
    }
}

void DecryptFolderTask(std::wstring param)
{
    _Params list;
    ExtractParams(param, list, 2);

    if (list.size() < 2 || list.size() > 3)
    {
        Throw(L"Params must be <Password> <Path 1> <Path 2> in which:\n\t\
<Path 1>\t: path to an encrypted folder or a  *.ef  file.\n\t<Path 2>\t\t: path to save the decrypted folder (optional).");
    }

    try
    {
        if (fs::is_directory(list[1]))
        {
            std::wstring path = L"";
            std::wstring name = L"";
            if (list.size() == 3)
            {
                path = CombinePath(currentDir, list[2]);
                name = DecryptFolder(list[1], CombinePath(currentDir, list[2]), WStringToString(list[0]));
            }
            else if (list.size() == 2)
            {
                path = currentDir;
                name =  DecryptFolder(list[1], currentDir, WStringToString(list[0]));
            }

            std::wcout << "Decrypted folder to \"" << path << L'\\' << name << L"\".\n";

        }
        else
        {
            std::wstring path = L"";
            std::wstring name = L"";
        
            auto pass = InputPassword();

            if(list.size() == 2)
            {
                path = CombinePath(currentDir, list[1]);
                //name = DecryptFolderFromFile(list[1], path, WStringToString(list[0]));
                auto ef = FolderToSingleFileEncryption::OpenEncryptedFolder(list[0], pass);
                auto folder = FolderToSingleFileEncryption::DecryptFolder(ef, path, pass);

                name = folder->Name();

                delete folder;
                delete ef;
            }
            else if (list.size() == 1)
            {
                path = currentDir;
                //name = DecryptFolderFromFile(list[1], currentDir, WStringToString(list[0]));
                auto ef = FolderToSingleFileEncryption::OpenEncryptedFolder(list[0], pass);
                auto folder = FolderToSingleFileEncryption::DecryptFolder(ef, currentDir, pass);

                name = folder->Name();

                delete folder;
                delete ef;
            }

            std::wcout << "Decrypted folder to \"" << path << L'\\' << name << L"\".\n";
        }
    }
    catch (...)
    {
        throw L"Wrong password.";
    }
    
}


void ExcuteCmd(const std::wstring& cmd)
{
    std::wstringstream ss(cmd);

    std::wstring task = L"";
    ss >> task;
    if (task.empty()) return;
    ToLower(task);

    auto cur = task;
    if (!_taskSuffix.empty()) cur += L" " + _taskSuffix;
    auto it = taskMap.find(cur);

    if (it != taskMap.end())
    {
        auto index = cmd.find_first_of(L" ");
        if (index == std::wstring::npos)
        {
            it->second(L" ");
        }
        else
        {
            auto param = cmd.substr(index + 1);
            StandardPath(param);
            it->second(param);
        }
    }
    else
    {
        throw task + L": command not found ! Type \"help\" to show help !";
    }

}

int PrepareConsole()
{
#ifdef _WIN32
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return GetLastError();
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return GetLastError();
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode))
    {
        return GetLastError();
    }

#endif
    return 1;
}


int main()
{
    PrepareConsole();

    auto lo = std::wcout.imbue(std::locale("en_US.utf8"));

    //some funny
    taskMap.insert({ L"cd", ChangeDirectory });
    taskMap.insert({ L"show", ShowFolderTree });
    //taskMap.insert({ L"print-structure", PrintStructureFile });
    //taskMap.insert({ L"save-structure", SaveFolderStructure });

    //encrypt and decrypt
    taskMap.insert({ L"encrypt-folder", EncryptFolderTask });
    taskMap.insert({ L"open", OpenEncryptedFolderTask });

    taskMap.insert({ L"decrypt-folder", DecryptFolderTask });

    //taskMap.insert({ L"password",[](std::wstring param) { InputPassword(); } });


    //simulate the folder to single file encrypted as a normal folder
    taskMap.insert({ L"cd -ef", ef::ChangeDirectory });
    taskMap.insert({ L"show -ef", ef::Show });
    taskMap.insert({ L"summary -ef", ef::Summary });

    taskMap.insert({ L"open -ef", ef::OpenFile });

    taskMap.insert({ L"add -ef", ef::Add });
    taskMap.insert({ L"delete -ef", ef::Delete });

    taskMap.insert({ L"decrypt -ef", ef::Decrypt });

    taskMap.insert({ L"close -ef", [](std::wstring param) { ef::Close(); } });

    taskMap.insert({ L"quit -ef", [](std::wstring param) { ef::Close(); exit(0); } });
    

    taskMap.insert({ L"quit", [](std::wstring param) { exit(0); } });

    wchar_t buffer[_MAX_DIR] = {};
    GetCurrentDirectoryW(_MAX_DIR, buffer);
    currentDir = buffer;

    std::wstring cmd;
    cmd.resize(1024);

#ifdef _WIN32
    void* con = GetStdHandle(STD_INPUT_HANDLE);
    unsigned long read = 0;
#endif // _WIN32


    while (true)
    {
        try
        {
            std::wcout << currentDir << ">";

#ifdef _WIN32
            ReadConsole(con, &cmd[0], 1024, &read, NULL);
            if (read != 0)
            {
                cmd[read - 1] = L'\0';
                if (read > 1)
                {
                    if (cmd[read - 2] == L'\r') cmd[read - 2] = L'\0';
                }
            }
            
#else
            std::getline(std::wcin, cmd);
#endif // _WIN32


            std::wcout << L'\n';

            ExcuteCmd(cmd.c_str());

            std::wcout << L'\n';

        }
        catch (const std::wstring e1)
        {
            std::wcerr << e1 << "\n\n";
        }
        catch (const wchar_t* e2)
        {
            std::wcerr << e2 << "\n\n";
        }
        catch (const char* e3)
        {
            std::cerr << e3 << "\n\n";
        }
    }

    return 0;
}