#pragma once
#include <iostream>
#include <string>

#include "../Common/CommonFuction.h"
#include "../Common/File.h"

#include "FolderToSingleFileEncryption.h"

#include "FileFolder.h"

std::string InputPassword();

#define SafeDelete(p) if(p) { delete p; p = nullptr; }

extern std::wstring currentDir;

extern std::wstring _taskSuffix;

extern FolderToSingleFileEncryption::EncryptedFolder* curEncryptedFolder;

void ExtractParams(std::wstring & in_str, std::vector<std::wstring> & out, size_t num);

namespace FolderToSingleFileEncryption
{


inline auto GetCurrentFolder()
{
    Folder<size_t>* folder = nullptr;

    if (curEncryptedFolder->m_path == currentDir)
    {
        folder = curEncryptedFolder->m_root;
    }
    else
    {
        folder = curEncryptedFolder->m_root->GetFolder(MakeRelativePath(curEncryptedFolder->m_path, currentDir));
    }

    return folder;
}

inline auto Open(const std::wstring& path)
{
    return OpenEncryptedFolder(path, InputPassword());
}

inline auto SaveChanges(std::string key = "")
{
    if (!curEncryptedFolder->Changed()) return;
    std::wstring ch;
    std::wcout << L"Save changes ? (y/n): ";
    std::getline(std::wcin, ch);
    ToLower(ch);
    if (L"y" == ch || L"yes" == ch)
    {
        if (key.empty())
        {
            key = InputPassword();
            if (!curEncryptedFolder->VerifyKey(key)) Throw(L"Wrong password.");
        }
        curEncryptedFolder->Save(key);
        std::wcout << L"\"" + curEncryptedFolder->m_name + L"\" saved.\n";
    }
}

inline auto Close()
{
    std::wcout << L"Close \"" + curEncryptedFolder->m_name + L"\" ? (y/n): ";

    std::wstring ch;
    std::getline(std::wcin, ch);
    ToLower(ch);

    if (L"y" == ch || L"yes" == ch)
    {
        SaveChanges();
        std::wcout << L"\"" + curEncryptedFolder->m_name + L"\" closed.\n";
        currentDir = currentDir.substr(0, currentDir.find(curEncryptedFolder->m_name) - 1);
        SafeDelete(curEncryptedFolder);
        _taskSuffix.clear();

        SetCurrentDir(currentDir);
    }
}

inline void ChangeDirectory(std::wstring param)
{
    size_t index = param.find(curEncryptedFolder->m_path);
    bool isRelPath = IsValidRelativePath(param);

    if (index == std::wstring::npos && !isRelPath)
    {
        Close();
    }
    else
    {
        if (isRelPath)
        {
            auto path = CombinePath(currentDir, param);

            size_t index2 = path.find(curEncryptedFolder->m_name);

            if (index2 == std::wstring::npos)
            {
                Close();
            }
            else
            {
                auto folder = curEncryptedFolder->m_root->GetFolder(MakeRelativePath(curEncryptedFolder->m_path, path));
                if (folder) currentDir = path;
                else Throw(L"\"" + param + L"\" not found.");
            }
        }
    }
}

inline void Show(std::wstring param)
{
    Folder<size_t>* folder = GetCurrentFolder();

    if(folder) std::wcout << folder << '\n';
}

inline void Summary(std::wstring param)
{
    Folder<size_t>* folder = GetCurrentFolder();

    if (folder)
    {
        size_t totalFolder = 0;
        size_t totalFile = 0;
        size_t totalFileSize = 0;

        folder->Traverse(
            [&](Folder<size_t>* current) 
            {
                if(current != folder) totalFolder++;
                totalFile += current->FileCount();

                for (auto& f : current->Files())
                {
                    totalFileSize += curEncryptedFolder->GetFileSize(f);
                }
                
            }, 
            nullptr
        );

        std::wcout << L"Name\t\t:  " << folder->Name() << L"\n";
        std::wcout << L"Contains\t:  " << totalFile << L" Files, " << totalFolder << L" Folders\n"
            << L"Size\t\t:  " << totalFileSize << L" bytes\n";
    }
}

inline void OpenFile(std::wstring param)
{
    if (IsBlank(param))
    {
        Throw(L"Param must be <path>:\n\t<path>\t: path to file in the encrypted folder.");
    }

    auto pass = InputPassword();

    if (!curEncryptedFolder->VerifyKey(pass)) Throw(L"Wrong password.");

#ifdef _WIN32
    auto file = curEncryptedFolder->FindFile(CombinePath(currentDir, param));

    auto fileName = file->Name();

    if (!file) Throw(L"File not found.");

    std::wstring name;

    auto path = currentDir.substr(0, currentDir.find(curEncryptedFolder->m_name) - 1);

    name = GenerateTempFilePath(path);
    name += fileName;

    pass.resize(32);

    std::fstream fstream(name, std::ios::binary | std::ios::out);

    int attr = GetFileAttributes(name.c_str());
    if ((attr & FILE_ATTRIBUTE_HIDDEN) == 0) 
    {
        SetFileAttributes(name.c_str(), attr | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY);
    }

    curEncryptedFolder->DecryptFile(file, fstream, pass);

    fstream.close();

    _wsystem(&name[0]);
    system("pause");

    DeleteFile(&name[0]);
#else
    Throw(L"\"open\" command support for Windows platfrom currently.");
#endif // _WIN32

}

inline void AddFile(_Params& list)
{
    auto pass = InputPassword();

    if (!curEncryptedFolder->VerifyKey(pass)) Throw(L"Wrong password.");

    auto name = GetFileName(list[0]);

    if (list.size() == 2)
    {
        try
        {
            auto to = CombinePath(currentDir, list[1]);
            curEncryptedFolder->AddNewFile(list[0], to, pass);
            std::wcout << L"File \"" << name << L"\" added to \"" << to << "\".\n";
        }
        catch (...)
        {
            if (Confirm(L"File \"" + name + L"\" exists. Replace destination ?"))
            {
                curEncryptedFolder->AddFileReplaceExists(list[0], CombinePath(currentDir, list[1]), pass);
                std::wcout << L"Replaced destination.\n";
            }
        }
    }
    else if (list.size() == 1)
    {
        try
        {
            curEncryptedFolder->AddNewFile(list[0], currentDir, pass);
            std::wcout << L"File \"" << name << L"\" added to \"" << currentDir << "\".\n";
        }
        catch (...)
        {
            if (Confirm(L"File \"" + name + L"\" exists. Replace destination ?"))
            {
                curEncryptedFolder->AddFileReplaceExists(list[0], currentDir, pass);
                std::wcout << L"Replaced destination.\n";
            }
        }
    }
    
    SaveChanges(pass);

}

inline void AddFolder(_Params& list)
{
    auto pass = InputPassword();

    if (!curEncryptedFolder->VerifyKey(pass)) Throw(L"Wrong password.");

    if (list[0].back() == L'\\') list[0].pop_back();

    auto name = GetFileName(list[0]);

    curEncryptedFolder->AddFolder(list[0], list[1], pass, 
        [](File<size_t>* file, Folder<size_t>* folder)->bool 
        {
            if (Confirm(L"File \"" + file->Name() + L"\" exists in folder \"" + folder->Path() + L"\". Replace destination ?"))
            {
                return true;
            }
            return false;
        });

    std::wcout << L"Folder \"" << name << L"\" added to \"" << list[1] << L"\".\n";

    SaveChanges(pass);

}

inline void Add(std::wstring param)
{
    _Params list;
    ExtractParams(param, list, 2);

    if (list.size() != 2 && list.size() != 1)
    {
        Throw(L"Params must be <from> <to>:\n\t<from>\t: path to file/folder in storage.\n\t<to>  \t: a folder in the encrypted folder (optional).");
    }

    if (fs::is_directory(list[0]))
    {
        if (list.size() == 1) list.push_back(currentDir);
        else list[1] = CombinePath(currentDir, list[1]);

        AddFolder(list);
    }
    else
    {
        AddFile(list);
    }

}

inline void Delete(std::wstring param)
{
    if (IsBlank(param))
    {
        Throw(L"Param must be <path>:\n\t<path>\t: path to file/folder will be deleted.");
    }

    auto pass = InputPassword();

    if (!curEncryptedFolder->VerifyKey(pass)) Throw(L"Wrong password.");

    auto name = GetFileName(param);
    auto path = CombinePath(currentDir, param);

    if (Confirm(L"\"" + name + L"\" will be deleted permanently. Continue ?"))
    {
        if (curEncryptedFolder->RemoveFile(path))
        {
            std::wcout << L"File \"" << name << L"\" deleted.\n";
        }
        else if (curEncryptedFolder->RemoveFolder(path))
        {
            std::wcout << L"Folder \"" << name << L"\" deleted.\n";
        }
        else
        {
            Throw(L"\"" + name + L"\" not found.");
        }
    }
    else
    {
        std::wcout << L"Delete cancel.\n";
    }

    SaveChanges(pass);
    
}

inline void Decrypt(std::wstring param)
{
    _Params list;
    ExtractParams(param, list, 2);

    auto outPath = CombinePath(currentDir, list[1]);

    if ((list.size() != 2 && list.size() != 1) || !fs::is_directory(outPath))
    {
        Throw(L"Params must be <from> <to>:\n\t<from>\t: path to file/folder in the encrypted folder (optional).\n\t<to>  \t: a folder in storage.");
    }

    auto pass = InputPassword();

    if (!curEncryptedFolder->VerifyKey(pass)) Throw(L"Wrong password.");

    auto name = GetFileName(param);
    auto path = CombinePath(currentDir, param);

    if (list.size() == 1)
    {
        list.push_back(currentDir);
        std::swap(list[0], list[1]);
    }
    

    auto srcPath = CombinePath(currentDir, list[0]);

    auto folder = curEncryptedFolder->FindFolder(srcPath);

    pass.resize(32);
    
    if (folder)
    {
        curEncryptedFolder->DecryptFolder(folder, outPath, pass);
        std::wcout << L"Folder \"" << folder->Name() << L"\" decrypted to \"" << outPath << L"\".\n";
    }
    else
    {
        auto file = curEncryptedFolder->FindFile(srcPath);

        if (file)
        {
            std::fstream fout(outPath + L'\\' + file->Name(), std::ios::binary | std::ios::out);
            curEncryptedFolder->DecryptFile(file, fout, pass);
            fout.close();

            std::wcout << L"File \"" << file->Name() << L"\" decrypted to \"" << outPath << L"\".\n";
        }
        else
        {
            Throw(L"File/Folder not found.");
        }
    }

   

}

}